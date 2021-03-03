#include "MIRCodegen.h"

#include <range/v3/algorithm/reverse.hpp>

namespace mir::bytecode_codegen {

/////////////////////////////// EntityMap //////////////////////////////////////
template <ranges::random_access_range T, typename IndexType>
typename T::value_type
EntityMap::getPtr(T const &Container, IndexType Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  if (!(CastedIndex < Container.size())) return nullptr;
  return Container[CastedIndex];
}

namespace {
namespace detail {
template <typename MEntityType, typename BEntityType>
void setImportExportInfo(MEntityType &MEntity, BEntityType const &BEntity) {
  if (BEntity.isImported()) {
    std::string ModuleName(BEntity.getImportModuleName());
    std::string EntityName(BEntity.getImportEntityName());
    MEntity->setImport(std::move(ModuleName), std::move(EntityName));
  }
  if (BEntity.isExported()) {
    std::string EntityName(BEntity.getExportName());
    MEntity->setExport(std::move(EntityName));
  }
}
} // namespace detail
} // namespace

EntityMap::EntityMap(bytecode::Module const &BModule, Module &MModule) {
  bytecode::ModuleView BModuleView(BModule);
  for (auto const &BMemory : BModuleView.memories()) {
    auto *MMemory = MModule.BuildMemory(*BMemory.getType());
    detail::setImportExportInfo(MMemory, BMemory);
    Memories.push_back(MMemory);
  }
  for (auto const &BTable : BModuleView.tables()) {
    auto *MTable = MModule.BuildTable(*BTable.getType());
    detail::setImportExportInfo(MTable, BTable);
    Tables.push_back(MTable);
  }
  for (auto const &BGlobal : BModuleView.globals()) {
    auto *MGlobal = MModule.BuildGlobal(*BGlobal.getType());
    detail::setImportExportInfo(MGlobal, BGlobal);
    Globals.push_back(MGlobal);
  }
  for (auto const &BFunction : BModuleView.functions()) {
    auto *MFunction = MModule.BuildFunction(*BFunction.getType());
    detail::setImportExportInfo(MFunction, BFunction);
    Functions.push_back(MFunction);
  }
}

Function *EntityMap::operator[](bytecode::FuncIDX Index) const {
  return getPtr(Functions, Index);
}

Memory *EntityMap::operator[](bytecode::MemIDX Index) const {
  return getPtr(Memories, Index);
}

Table *EntityMap::operator[](bytecode::TableIDX Index) const {
  return getPtr(Tables, Index);
}

Global *EntityMap::operator[](bytecode::GlobalIDX Index) const {
  return getPtr(Globals, Index);
}

void EntityMap::annotate(parser::customsections::Name const &Name) {
  for (auto const &FuncNameEntry : Name.getFunctionNames()) {
    auto *MFunction = operator[](FuncNameEntry.FuncIndex);
    MFunction->setName(FuncNameEntry.Name);
  }
  for (auto const &LocalNameEntry : Name.getLocalNames()) {
    auto *MFunction = operator[](LocalNameEntry.FuncIndex);
    auto &MLocal = *std::next(
        MFunction->local_begin(),
        static_cast<std::size_t>(LocalNameEntry.LocalIndex));
    MLocal.setName(LocalNameEntry.Name);
  }
}

Memory *EntityMap::getImplicitMemory() const {
  assert(!Memories.empty());
  return Memories.front();
}

Table *EntityMap::getImplicitTable() const {
  assert(!Tables.empty());
  return Tables.front();
}

//////////////////////// TranslationContext ////////////////////////////////////
TranslationTask::TranslationTask(
    bytecode::views::Function FunctionView_, Function &MFunction_)
    : FunctionView(FunctionView_), MFunction(MFunction_) {
  // Only function definition needs translation
  assert(!FunctionView_.isImported());
}

Local *TranslationTask::operator[](bytecode::LocalIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Locals.size());
  return Locals[CastedIndex];
}

void TranslationTask::LabelStackAccessView::enter(
    BasicBlock *MergeBasicBlock, BasicBlock *NextBasicBlock,
    unsigned int NumMergePhi) {
  CTX.Scopes.push_back(ScopeFrame{
      .MergeBB = MergeBasicBlock,
      .NextBB = NextBasicBlock,
      .NumMergePhi = NumMergePhi});
}

BasicBlock *TranslationTask::LabelStackAccessView::getMergeBB(
    bytecode::LabelIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < CTX.Scopes.size());
  return CTX.Scopes[CTX.Scopes.size() - CastedIndex - 1].MergeBB;
}

unsigned int TranslationTask::LabelStackAccessView::getNumMergePhi(
    bytecode::LabelIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < CTX.Scopes.size());
  return CTX.Scopes[CTX.Scopes.size() - CastedIndex - 1].NumMergePhi;
}

BasicBlock *TranslationTask::LabelStackAccessView::exit() {
  assert(!CTX.Scopes.empty());
  auto *NextBB = CTX.Scopes.back().NextBB;
  CTX.Scopes.pop_back();
  return NextBB;
}

void TranslationTask::ValueStackAccessView::push(Instruction *Value) {
  CTX.Values.push_back(Value);
}

Instruction *TranslationTask::ValueStackAccessView::pop() {
  assert(!CTX.Values.empty());
  auto *Value = CTX.Values.back();
  CTX.Values.pop_back();
  return Value;
}

//////////////////////////////// translate /////////////////////////////////////
namespace {
namespace detail {
using namespace bytecode::valuetypes;
namespace minsts = mir::instructions;
namespace binsts = bytecode::instructions;

class TranslateVisitor :
    public bytecode::InstVisitorBase<TranslateVisitor, void> {
  EntityMap const &EMap;
  TranslationTask &Context;

public:
  TranslateVisitor(EntityMap const &EMap_, TranslationTask &Context_)
      : EMap(EMap_), Context(Context_) {}

  void operator()(binsts::Unreachable const *) {
    Context.currentBB()->BuildInst<minsts::Unreachable>();
  }

  void operator()(binsts::Nop const *) {}

  void operator()(binsts::Drop const *) { Context.values().pop(); }

  void operator()(binsts::Select const *) {
    auto *BB = Context.currentBB();
    auto *Condition = Context.values().pop();
    auto *FalseValue = Context.values().pop();
    auto *TrueValue = Context.values().pop();
    auto *Result =
        BB->BuildInst<minsts::Select>(Condition, TrueValue, FalseValue);
    Context.values().push(Result);
  }

  void operator()(binsts::LocalGet const *Inst) {
    auto *Target = Context[Inst->Target];
    auto *Result = Context.currentBB()->BuildInst<minsts::LocalGet>(Target);
    Context.values().push(Result);
  }

  void operator()(binsts::LocalSet const *Inst) {
    auto *Target = Context[Inst->Target];
    auto *Operand = Context.values().pop();
    Context.currentBB()->BuildInst<minsts::LocalSet>(Target, Operand);
  }

  void operator()(binsts::LocalTee const *Inst) {
    auto *Target = Context[Inst->Target];
    auto *Operand = Context.values().pop();
    Context.currentBB()->BuildInst<minsts::LocalSet>(Target, Operand);
    Context.values().push(Operand);
  }

  void operator()(binsts::GlobalGet const *Inst) {
    auto *Target = EMap[Inst->Target];
    auto *Result = Context.currentBB()->BuildInst<minsts::GlobalGet>(Target);
    Context.values().push(Result);
  }

  void operator()(binsts::GlobalSet const *Inst) {
    auto *Target = EMap[Inst->Target];
    auto *Operand = Context.values().pop();
    Context.currentBB()->BuildInst<minsts::GlobalSet>(Target, Operand);
  }

#define LOAD_ZERO_EXTEND(BYTECODE_INST, LOAD_TYPE, LOAD_WIDTH)                 \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *BB = Context.currentBB();                                            \
    auto *Address = Context.values().pop();                                    \
    auto *Memory = EMap.getImplicitMemory();                                   \
    BB->BuildInst<minsts::MemoryGuard>(Memory, Address, LOAD_WIDTH);           \
    auto *Result =                                                             \
        BB->BuildInst<minsts::Load>(Memory, LOAD_TYPE, Address, LOAD_WIDTH);   \
    Context.values().push(Result);                                             \
  }
  // clang-format off
  LOAD_ZERO_EXTEND(binsts::I32Load   , I32, 32)
  LOAD_ZERO_EXTEND(binsts::I64Load   , I64, 64)
  LOAD_ZERO_EXTEND(binsts::F32Load   , F32, 32)
  LOAD_ZERO_EXTEND(binsts::F64Load   , F64, 64)
  LOAD_ZERO_EXTEND(binsts::I32Load8U , I32, 8 )
  LOAD_ZERO_EXTEND(binsts::I32Load16U, I32, 16)
  LOAD_ZERO_EXTEND(binsts::I64Load8U , I64, 8 )
  LOAD_ZERO_EXTEND(binsts::I64Load16U, I64, 16)
  LOAD_ZERO_EXTEND(binsts::I64Load32U, I64, 32)
  // clang-format on
#undef LOAD_ZERO_EXTEND

#define LOAD_SIGN_EXTEND(BYTECODE_INST, LOAD_TYPE, LOAD_WIDTH)                 \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *BB = Context.currentBB();                                            \
    auto *Address = Context.values().pop();                                    \
    auto *Memory = EMap.getImplicitMemory();                                   \
    BB->BuildInst<minsts::MemoryGuard>(Memory, Address, LOAD_WIDTH);           \
    auto *Result =                                                             \
        BB->BuildInst<minsts::Load>(Memory, LOAD_TYPE, Address, LOAD_WIDTH);   \
    auto *ExtendResult = BB->BuildInst<minsts::Extend>(Result, LOAD_WIDTH);    \
    Context.values().push(ExtendResult);                                       \
  }
  // clang-format off
  LOAD_SIGN_EXTEND(binsts::I32Load8S , I32, 8 )
  LOAD_SIGN_EXTEND(binsts::I32Load16S, I32, 16)
  LOAD_SIGN_EXTEND(binsts::I64Load8S , I64, 8 )
  LOAD_SIGN_EXTEND(binsts::I64Load16S, I64, 16)
  LOAD_SIGN_EXTEND(binsts::I64Load32S, I64, 32)
  // clang-format on
#undef LOAD_SIGN_EXTEND

  void operator()(binsts::MemorySize const *) {
    auto *Memory = EMap.getImplicitMemory();
    auto *Result = Context.currentBB()->BuildInst<minsts::MemorySize>(Memory);
    Context.values().push(Result);
  }

  void operator()(binsts::MemoryGrow const *) {
    auto *Operand = Context.values().pop();
    auto *Memory = EMap.getImplicitMemory();
    auto *Result =
        Context.currentBB()->BuildInst<minsts::MemoryGrow>(Memory, Operand);
    Context.values().push(Result);
  }

#define CONSTANT(BYTECODE_INST)                                                \
  void operator()(BYTECODE_INST const *Inst) {                                 \
    auto Imm = Inst->Value;                                                    \
    auto *Result = Context.currentBB()->BuildInst<minsts::Constant>(Imm);      \
    Context.values().push(Result);                                             \
  }
  CONSTANT(binsts::I32Const)
  CONSTANT(binsts::I64Const)
  CONSTANT(binsts::F32Const)
  CONSTANT(binsts::F64Const)
#undef CONSTANT

#define INT_UNARY_OP(BYTECODE_INST, MIR_INST_OPERATOR)                         \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *Operand = Context.values().pop();                                    \
    auto Operator = MIR_INST_OPERATOR;                                         \
    auto *Result =                                                             \
        Context.currentBB()->BuildInst<minsts::IntUnaryOp>(Operator, Operand); \
    Context.values().push(Result);                                             \
  }
  // clang-format off
  INT_UNARY_OP(binsts::I32Eqz   , minsts::IntUnaryOperator::Eqz   )
  INT_UNARY_OP(binsts::I32Clz   , minsts::IntUnaryOperator::Clz   )
  INT_UNARY_OP(binsts::I32Ctz   , minsts::IntUnaryOperator::Ctz   )
  INT_UNARY_OP(binsts::I32Popcnt, minsts::IntUnaryOperator::Popcnt)

  INT_UNARY_OP(binsts::I64Eqz   , minsts::IntUnaryOperator::Eqz   )
  INT_UNARY_OP(binsts::I64Clz   , minsts::IntUnaryOperator::Clz   )
  INT_UNARY_OP(binsts::I64Ctz   , minsts::IntUnaryOperator::Ctz   )
  INT_UNARY_OP(binsts::I64Popcnt, minsts::IntUnaryOperator::Popcnt)
  // clang-format on
#undef INT_UNARY_OP

#define INT_BINARY_OP(BYTECODE_INST, MIR_INST_OPERATOR)                        \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *BB = Context.currentBB();                                            \
    auto *RHS = Context.values().pop();                                        \
    auto *LHS = Context.values().pop();                                        \
    auto Operator = MIR_INST_OPERATOR;                                         \
    auto *Result = BB->BuildInst<minsts::IntBinaryOp>(Operator, LHS, RHS);     \
    Context.values().push(Result);                                             \
  }
  // clang-format off
  INT_BINARY_OP(binsts::I32Eq  , minsts::IntBinaryOperator::Eq  )
  INT_BINARY_OP(binsts::I32Ne  , minsts::IntBinaryOperator::Ne  )
  INT_BINARY_OP(binsts::I32LtS , minsts::IntBinaryOperator::LtS )
  INT_BINARY_OP(binsts::I32LtU , minsts::IntBinaryOperator::LtU )
  INT_BINARY_OP(binsts::I32GtS , minsts::IntBinaryOperator::GtS )
  INT_BINARY_OP(binsts::I32GtU , minsts::IntBinaryOperator::GtU )
  INT_BINARY_OP(binsts::I32LeS , minsts::IntBinaryOperator::LeS )
  INT_BINARY_OP(binsts::I32LeU , minsts::IntBinaryOperator::LeU )
  INT_BINARY_OP(binsts::I32GeS , minsts::IntBinaryOperator::GeS )
  INT_BINARY_OP(binsts::I32GeU , minsts::IntBinaryOperator::GeU )
  INT_BINARY_OP(binsts::I32Add , minsts::IntBinaryOperator::Add )
  INT_BINARY_OP(binsts::I32Sub , minsts::IntBinaryOperator::Sub )
  INT_BINARY_OP(binsts::I32Mul , minsts::IntBinaryOperator::Mul )
  INT_BINARY_OP(binsts::I32DivS, minsts::IntBinaryOperator::DivS)
  INT_BINARY_OP(binsts::I32DivU, minsts::IntBinaryOperator::DivU)
  INT_BINARY_OP(binsts::I32RemS, minsts::IntBinaryOperator::RemS)
  INT_BINARY_OP(binsts::I32RemU, minsts::IntBinaryOperator::RemU)
  INT_BINARY_OP(binsts::I32And , minsts::IntBinaryOperator::And )
  INT_BINARY_OP(binsts::I32Or  , minsts::IntBinaryOperator::Or  )
  INT_BINARY_OP(binsts::I32Xor , minsts::IntBinaryOperator::Xor )
  INT_BINARY_OP(binsts::I32Shl , minsts::IntBinaryOperator::Shl )
  INT_BINARY_OP(binsts::I32ShrS, minsts::IntBinaryOperator::ShrS)
  INT_BINARY_OP(binsts::I32ShrU, minsts::IntBinaryOperator::ShrU)
  INT_BINARY_OP(binsts::I32Rotl, minsts::IntBinaryOperator::Rotl)
  INT_BINARY_OP(binsts::I32Rotr, minsts::IntBinaryOperator::Rotr)

  INT_BINARY_OP(binsts::I64Eq  , minsts::IntBinaryOperator::Eq  )
  INT_BINARY_OP(binsts::I64Ne  , minsts::IntBinaryOperator::Ne  )
  INT_BINARY_OP(binsts::I64LtS , minsts::IntBinaryOperator::LtS )
  INT_BINARY_OP(binsts::I64LtU , minsts::IntBinaryOperator::LtU )
  INT_BINARY_OP(binsts::I64GtS , minsts::IntBinaryOperator::GtS )
  INT_BINARY_OP(binsts::I64GtU , minsts::IntBinaryOperator::GtU )
  INT_BINARY_OP(binsts::I64LeS , minsts::IntBinaryOperator::LeS )
  INT_BINARY_OP(binsts::I64LeU , minsts::IntBinaryOperator::LeU )
  INT_BINARY_OP(binsts::I64GeS , minsts::IntBinaryOperator::GeS )
  INT_BINARY_OP(binsts::I64GeU , minsts::IntBinaryOperator::GeU )
  INT_BINARY_OP(binsts::I64Add , minsts::IntBinaryOperator::Add )
  INT_BINARY_OP(binsts::I64Sub , minsts::IntBinaryOperator::Sub )
  INT_BINARY_OP(binsts::I64Mul , minsts::IntBinaryOperator::Mul )
  INT_BINARY_OP(binsts::I64DivS, minsts::IntBinaryOperator::DivS)
  INT_BINARY_OP(binsts::I64DivU, minsts::IntBinaryOperator::DivU)
  INT_BINARY_OP(binsts::I64RemS, minsts::IntBinaryOperator::RemS)
  INT_BINARY_OP(binsts::I64RemU, minsts::IntBinaryOperator::RemU)
  INT_BINARY_OP(binsts::I64And , minsts::IntBinaryOperator::And )
  INT_BINARY_OP(binsts::I64Or  , minsts::IntBinaryOperator::Or  )
  INT_BINARY_OP(binsts::I64Xor , minsts::IntBinaryOperator::Xor )
  INT_BINARY_OP(binsts::I64Shl , minsts::IntBinaryOperator::Shl )
  INT_BINARY_OP(binsts::I64ShrS, minsts::IntBinaryOperator::ShrS)
  INT_BINARY_OP(binsts::I64ShrU, minsts::IntBinaryOperator::ShrU)
  INT_BINARY_OP(binsts::I64Rotl, minsts::IntBinaryOperator::Rotl)
  INT_BINARY_OP(binsts::I64Rotr, minsts::IntBinaryOperator::Rotr)
  // clang-format on
#undef INT_BINARY_OP

#define FP_UNARY_OP(BYTECODE_INST, MIR_INST_OPERATOR)                          \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *Operand = Context.values().pop();                                    \
    auto Operator = MIR_INST_OPERATOR;                                         \
    auto *Result =                                                             \
        Context.currentBB()->BuildInst<minsts::FPUnaryOp>(Operator, Operand);  \
    Context.values().push(Result);                                             \
  }
  // clang-format off
  FP_UNARY_OP(binsts::F32Abs    , minsts::FPUnaryOperator::Abs    )
  FP_UNARY_OP(binsts::F32Neg    , minsts::FPUnaryOperator::Neg    )
  FP_UNARY_OP(binsts::F32Ceil   , minsts::FPUnaryOperator::Ceil   )
  FP_UNARY_OP(binsts::F32Floor  , minsts::FPUnaryOperator::Floor  )
  FP_UNARY_OP(binsts::F32Trunc  , minsts::FPUnaryOperator::Trunc  )
  FP_UNARY_OP(binsts::F32Nearest, minsts::FPUnaryOperator::Nearest)
  FP_UNARY_OP(binsts::F32Sqrt   , minsts::FPUnaryOperator::Sqrt   )

  FP_UNARY_OP(binsts::F64Abs    , minsts::FPUnaryOperator::Abs    )
  FP_UNARY_OP(binsts::F64Neg    , minsts::FPUnaryOperator::Neg    )
  FP_UNARY_OP(binsts::F64Ceil   , minsts::FPUnaryOperator::Ceil   )
  FP_UNARY_OP(binsts::F64Floor  , minsts::FPUnaryOperator::Floor  )
  FP_UNARY_OP(binsts::F64Trunc  , minsts::FPUnaryOperator::Trunc  )
  FP_UNARY_OP(binsts::F64Nearest, minsts::FPUnaryOperator::Nearest)
  FP_UNARY_OP(binsts::F64Sqrt   , minsts::FPUnaryOperator::Sqrt   )
  // clang-format on
#undef FP_UNARY_OP

#define FP_BINARY_OP(BYTECODE_INST, MIR_INST_OPERATOR)                         \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *BB = Context.currentBB();                                            \
    auto *RHS = Context.values().pop();                                        \
    auto *LHS = Context.values().pop();                                        \
    auto Operator = MIR_INST_OPERATOR;                                         \
    auto *Result = BB->BuildInst<minsts::FPBinaryOp>(Operator, LHS, RHS);      \
    Context.values().push(Result);                                             \
  }
  // clang-format off
  FP_BINARY_OP(binsts::F32Eq      , minsts::FPBinaryOperator::Eq      )
  FP_BINARY_OP(binsts::F32Ne      , minsts::FPBinaryOperator::Ne      )
  FP_BINARY_OP(binsts::F32Lt      , minsts::FPBinaryOperator::Lt      )
  FP_BINARY_OP(binsts::F32Gt      , minsts::FPBinaryOperator::Gt      )
  FP_BINARY_OP(binsts::F32Le      , minsts::FPBinaryOperator::Le      )
  FP_BINARY_OP(binsts::F32Ge      , minsts::FPBinaryOperator::Ge      )
  FP_BINARY_OP(binsts::F32Add     , minsts::FPBinaryOperator::Add     )
  FP_BINARY_OP(binsts::F32Sub     , minsts::FPBinaryOperator::Sub     )
  FP_BINARY_OP(binsts::F32Mul     , minsts::FPBinaryOperator::Mul     )
  FP_BINARY_OP(binsts::F32Div     , minsts::FPBinaryOperator::Div     )
  FP_BINARY_OP(binsts::F32Min     , minsts::FPBinaryOperator::Min     )
  FP_BINARY_OP(binsts::F32Max     , minsts::FPBinaryOperator::Max     )
  FP_BINARY_OP(binsts::F32CopySign, minsts::FPBinaryOperator::CopySign)

  FP_BINARY_OP(binsts::F64Eq      , minsts::FPBinaryOperator::Eq      )
  FP_BINARY_OP(binsts::F64Ne      , minsts::FPBinaryOperator::Ne      )
  FP_BINARY_OP(binsts::F64Lt      , minsts::FPBinaryOperator::Lt      )
  FP_BINARY_OP(binsts::F64Gt      , minsts::FPBinaryOperator::Gt      )
  FP_BINARY_OP(binsts::F64Le      , minsts::FPBinaryOperator::Le      )
  FP_BINARY_OP(binsts::F64Ge      , minsts::FPBinaryOperator::Ge      )
  FP_BINARY_OP(binsts::F64Add     , minsts::FPBinaryOperator::Add     )
  FP_BINARY_OP(binsts::F64Sub     , minsts::FPBinaryOperator::Sub     )
  FP_BINARY_OP(binsts::F64Mul     , minsts::FPBinaryOperator::Mul     )
  FP_BINARY_OP(binsts::F64Div     , minsts::FPBinaryOperator::Div     )
  FP_BINARY_OP(binsts::F64Min     , minsts::FPBinaryOperator::Min     )
  FP_BINARY_OP(binsts::F64Max     , minsts::FPBinaryOperator::Max     )
  FP_BINARY_OP(binsts::F64CopySign, minsts::FPBinaryOperator::CopySign)
  // clang-format on
#undef FP_BINARY_OP

#define CAST(BYTECODE_INST, MODE, TARGET_TYPE)                                 \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *BB = Context.currentBB();                                            \
    auto *Operand = Context.values().pop();                                    \
    auto *Result = BB->BuildInst<minsts::Cast>(MODE, TARGET_TYPE, Operand);    \
    Context.values().push(Result);                                             \
  }
  // clang-format off
  CAST(binsts::I32WrapI64       , minsts::CastMode::Conversion           , I32)
  CAST(binsts::I32TruncF32S     , minsts::CastMode::ConversionSigned     , I32)
  CAST(binsts::I32TruncF32U     , minsts::CastMode::ConversionUnsigned   , I32)
  CAST(binsts::I32TruncF64S     , minsts::CastMode::ConversionSigned     , I32)
  CAST(binsts::I32TruncF64U     , minsts::CastMode::ConversionUnsigned   , I32)
  CAST(binsts::I64ExtendI32S    , minsts::CastMode::ConversionSigned     , I64)
  CAST(binsts::I64ExtendI32U    , minsts::CastMode::ConversionUnsigned   , I64)
  CAST(binsts::I64TruncF32S     , minsts::CastMode::ConversionSigned     , I64)
  CAST(binsts::I64TruncF32U     , minsts::CastMode::ConversionUnsigned   , I64)
  CAST(binsts::I64TruncF64S     , minsts::CastMode::ConversionSigned     , I64)
  CAST(binsts::I64TruncF64U     , minsts::CastMode::ConversionUnsigned   , I64)
  CAST(binsts::F32ConvertI32S   , minsts::CastMode::ConversionSigned     , F32)
  CAST(binsts::F32ConvertI32U   , minsts::CastMode::ConversionUnsigned   , F32)
  CAST(binsts::F32ConvertI64S   , minsts::CastMode::ConversionSigned     , F32)
  CAST(binsts::F32ConvertI64U   , minsts::CastMode::ConversionUnsigned   , F32)
  CAST(binsts::F32DemoteF64     , minsts::CastMode::Conversion           , F32)
  CAST(binsts::F64ConvertI32S   , minsts::CastMode::ConversionSigned     , F64)
  CAST(binsts::F64ConvertI32U   , minsts::CastMode::ConversionUnsigned   , F64)
  CAST(binsts::F64ConvertI64S   , minsts::CastMode::ConversionSigned     , F64)
  CAST(binsts::F64ConvertI64U   , minsts::CastMode::ConversionUnsigned   , F64)
  CAST(binsts::F64PromoteF32    , minsts::CastMode::Conversion           , F64)
  CAST(binsts::I32ReinterpretF32, minsts::CastMode::Reinterpret          , I32)
  CAST(binsts::I64ReinterpretF64, minsts::CastMode::Reinterpret          , I64)
  CAST(binsts::F32ReinterpretI32, minsts::CastMode::Reinterpret          , F32)
  CAST(binsts::F64ReinterpretI64, minsts::CastMode::Reinterpret          , F64)

  CAST(binsts::I32TruncSatF32S  , minsts::CastMode::SatConversionSigned  , I32)
  CAST(binsts::I32TruncSatF32U  , minsts::CastMode::SatConversionUnsigned, I32)
  CAST(binsts::I32TruncSatF64S  , minsts::CastMode::SatConversionSigned  , I32)
  CAST(binsts::I32TruncSatF64U  , minsts::CastMode::SatConversionUnsigned, I32)
  CAST(binsts::I64TruncSatF32S  , minsts::CastMode::SatConversionSigned  , I64)
  CAST(binsts::I64TruncSatF32U  , minsts::CastMode::SatConversionUnsigned, I64)
  CAST(binsts::I64TruncSatF64S  , minsts::CastMode::SatConversionSigned  , I64)
  CAST(binsts::I64TruncSatF64U  , minsts::CastMode::SatConversionUnsigned, I64)
  // clang-format on
#undef CAST

#define EXTEND_SIGNED(BYTECODE_INST, FROM_WIDTH)                               \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *Operand = Context.values().pop();                                    \
    auto *Result =                                                             \
        Context.currentBB()->BuildInst<minsts::Extend>(Operand, FROM_WIDTH);   \
    Context.values().push(Result);                                             \
  }
  // clang-format off
  EXTEND_SIGNED(binsts::I32Extend8S , 8 )
  EXTEND_SIGNED(binsts::I32Extend16S, 16)
  EXTEND_SIGNED(binsts::I64Extend8S , 8 )
  EXTEND_SIGNED(binsts::I64Extend16S, 16)
  EXTEND_SIGNED(binsts::I64Extend32S, 32)
  // clang-format on
#undef EXTEND_SIGNED

  template <bytecode::instruction T> void operator()(T const *) {
    SABLE_UNREACHABLE();
  }
};
} // namespace detail
} // namespace

Instruction *TranslationTask::collectReturn(unsigned NumReturnValue) {
  if (NumReturnValue == 1) return values().pop();
  std::vector<Instruction *> ReturnValues;
  ReturnValues.reserve(function().getType()->getResultTypes().size());
  for (unsigned I = 0; I < NumReturnValue; ++I)
    ReturnValues.push_back(values().pop());
  ranges::reverse(ReturnValues);
  auto *PackedResult = currentBB()->BuildInst<instructions::Pack>(ReturnValues);
  return PackedResult;
}

void TranslationTask::perform(EntityMap const &EntityMap_) {
  for (auto const &Local : function().getLocals()) {
    auto *MLocal = MFunction.BuildLocal(Local);
    Locals.push_back(MLocal);
  }
  CurrentBB = createBasicBlock();
  CurrentBB->setName("entry");
  ExitBB = createBasicBlock();
  ExitBB->setName("exit");

  detail::TranslateVisitor Visitor(EntityMap_, *this);
  for (auto const &InstPtr : *function().getBody()) Visitor.visit(InstPtr);

  if (!function().getType()->isVoidReturn()) {
    auto *ResultValue =
        collectReturn(function().getType()->getResultTypes().size());
    ReturnValueCandidates.push_back(ResultValue);
  }
  CurrentBB->BuildInst<instructions::Branch>(ExitBB);

  if (function().getType()->isVoidReturn()) { // This is a void function
    ExitBB->BuildInst<instructions::Return>();
  } else if (ReturnValueCandidates.size() == 1) {
    auto *ReturnValue = ReturnValueCandidates.front();
    ExitBB->BuildInst<instructions::Return>(ReturnValue);
  } else {
    assert(!ReturnValueCandidates.empty());
    auto *ReturnValue =
        ExitBB->BuildInst<instructions::Phi>(ReturnValueCandidates);
    ExitBB->BuildInst<instructions::Return>(ReturnValue);
  }
}
} // namespace mir::bytecode_codegen
