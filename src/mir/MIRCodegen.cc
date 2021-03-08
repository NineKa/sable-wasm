#include "MIRCodegen.h"

#include <range/v3/view/reverse.hpp>
#include <range/v3/view/zip.hpp>

namespace mir::bytecode_codegen {
using namespace bytecode::valuetypes;
namespace minsts = mir::instructions;
namespace binsts = bytecode::instructions;
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

EntityMap::EntityMap(bytecode::Module const &BModule, Module &MModule)
    : BModuleView(BModule) {
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

bytecode::FunctionType const *
EntityMap::operator[](bytecode::TypeIDX Index) const {
  return BModuleView[Index];
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

////////////////////////// TranslationTask /////////////////////////////////////
namespace {
namespace detail {
template <ranges::input_range T>
void addMergeCandidates(BasicBlock *MergeBlock, T const &PhiCandidates) {
  for (auto &&[Instruction, CandidateValue] :
       ranges::views::zip(ranges::views::all(*MergeBlock), PhiCandidates)) {
    auto *PhiNode = dyn_cast<minsts::Phi>(std::addressof(Instruction));
    PhiNode->addArgument(CandidateValue);
  }
}

bool isTerminatingInstruction(bytecode::Instruction const *Inst) {
  switch (Inst->getOpcode()) {
  case bytecode::Opcode::Unreachable:
  case bytecode::Opcode::Return:
  case bytecode::Opcode::Br:
  case bytecode::Opcode::BrTable: return true;
  default: return false;
  }
}
} // namespace detail
} // namespace

class TranslationTask::TranslationContext {
  EntityMap const &E;
  bytecode::views::Function SourceFunction;
  mir::Function &TargetFunction;
  std::vector<mir::Local *> Locals;
  BasicBlock *EntryBasicBlock;
  BasicBlock *ExitBasicBlock;

  // < Branch Target with Merge Phi Nodes, Num of Phi Nodes>
  std::vector<std::tuple<BasicBlock *, unsigned>> Labels;

public:
  TranslationContext(
      EntityMap const &EntitiesResolver_,
      bytecode::views::Function SourceFunction_, mir::Function &TargetFunction_)
      : E(EntitiesResolver_), SourceFunction(SourceFunction_),
        TargetFunction(TargetFunction_) {
    if constexpr (ranges::sized_range<decltype(SourceFunction.getLocals())>)
      Locals.reserve(ranges::size(SourceFunction.getLocals()));
    for (auto const &LocalType : SourceFunction.getLocals()) {
      auto *Local = TargetFunction.BuildLocal(LocalType);
      Locals.push_back(Local);
    }
    EntryBasicBlock = target().BuildBasicBlock();
    EntryBasicBlock->setName("entry");
    ExitBasicBlock = target().BuildBasicBlock();
    ExitBasicBlock->setName("exit");

    std::vector<Instruction *> ExitPhiNodes;
    ExitPhiNodes.reserve(source().getType()->getNumResult());
    for (auto const &ReturnValueType : source().getType()->getResultTypes()) {
      auto *ExitPhi = ExitBasicBlock->BuildInst<minsts::Phi>(ReturnValueType);
      ExitPhiNodes.push_back(ExitPhi);
    }
    if (source().getType()->isVoidResult()) {
      ExitBasicBlock->BuildInst<minsts::Return>();
    } else if (source().getType()->isSingleValueResult()) {
      ExitBasicBlock->BuildInst<minsts::Return>(ExitPhiNodes.front());
    } else {
      assert(source().getType()->isMultiValueResult());
      auto *Operand = ExitBasicBlock->BuildInst<minsts::Pack>(ExitPhiNodes);
      ExitBasicBlock->BuildInst<minsts::Return>(Operand);
    }
  }

  bytecode::views::Function source() const { return SourceFunction; }
  mir::Function &target() { return TargetFunction; }
  mir::Function &target() const { return TargetFunction; }
  BasicBlock *entry() const { return EntryBasicBlock; }
  BasicBlock *exit() const { return ExitBasicBlock; }

  Function *operator[](bytecode::FuncIDX Index) const { return E[Index]; }
  Memory *operator[](bytecode::MemIDX Index) const { return E[Index]; }
  Table *operator[](bytecode::TableIDX Index) const { return E[Index]; }
  Global *operator[](bytecode::GlobalIDX Index) const { return E[Index]; }
  bytecode::FunctionType const *operator[](bytecode::TypeIDX Index) const {
    return E[Index];
  }

  Local *operator[](bytecode::LocalIDX Index) const {
    auto CastedIndex = static_cast<std::size_t>(Index);
    assert(CastedIndex < Locals.size());
    return Locals[CastedIndex];
  }

  std::tuple<BasicBlock *, unsigned>
  operator[](bytecode::LabelIDX Index) const {
    auto CastedIndex = static_cast<std::size_t>(Index);
    assert(CastedIndex < Labels.size());
    return Labels[Labels.size() - CastedIndex - 1];
  }

  void push(BasicBlock *MergeBasicBlock, unsigned NumPhiNodes) {
    Labels.emplace_back(MergeBasicBlock, NumPhiNodes);
  }
  void pop() { Labels.pop_back(); }
};

class TranslationTask::TranslationVisitor :
    public bytecode::InstVisitorBase<TranslationVisitor, void> {
  BasicBlock *CurrentBasicBlock;
  BasicBlock *InsertBefore = nullptr;
  TranslationContext &Context;
  std::vector<Instruction *> Values;

  BasicBlock *createBasicBlock() {
    return Context.target().BuildBasicBlock(InsertBefore);
  }

  bytecode::FunctionType
  convertBlockResult(bytecode::BlockResultType const &Type) {
    if (std::holds_alternative<bytecode::TypeIDX>(Type))
      return *Context[std::get<bytecode::TypeIDX>(Type)];
    if (std::holds_alternative<bytecode::ValueType>(Type)) {
      auto Result = std::get<bytecode::ValueType>(Type);
      return bytecode::FunctionType({}, {Result});
    }
    if (std::holds_alternative<bytecode::BlockResultUnit>(Type))
      return bytecode::FunctionType({}, {});
    SABLE_UNREACHABLE();
  }

public:
  TranslationVisitor(
      TranslationContext &Context_, BasicBlock *CurrentBasicBlock_,
      BasicBlock *InsertBefore_ = nullptr)
      : CurrentBasicBlock(CurrentBasicBlock_), InsertBefore(InsertBefore_),
        Context(Context_) {}

  struct LabelAccessView {
    TranslationVisitor &Visitor;
    LabelAccessView(TranslationVisitor &Visitor_) : Visitor(Visitor_) {}
    std::tuple<BasicBlock *, unsigned> operator[](bytecode::LabelIDX Index) {
      return Visitor.Context[Index];
    }
    void push(BasicBlock *MergeSiteBB, unsigned NumPhiNodes) {
      return Visitor.Context.push(MergeSiteBB, NumPhiNodes);
    }
    void pop() { Visitor.Context.pop(); }
  };
  LabelAccessView labels() { return LabelAccessView(*this); }

  struct ValueAccessView {
    TranslationVisitor &Visitor;
    ValueAccessView(TranslationVisitor &Visitor_) : Visitor(Visitor_) {}
    void push(Instruction *Value) { Visitor.Values.push_back(Value); }
    template <ranges::input_range T> void push(T const &Values_) {
      for (auto &&Value : Values_) push(Value);
    }
    Instruction *pop() {
      assert(!Visitor.Values.empty());
      auto *Value = Visitor.Values.back();
      Visitor.Values.pop_back();
      return Value;
    }
    void pop(std::size_t NumValues) {
      for (std::size_t I = 0; I < NumValues; ++I) utility::ignore(pop());
    }
    std::span<Instruction *> peek(std::size_t NumValues = 1) {
      assert(NumValues <= Visitor.Values.size());
      auto *StartPtr =
          std::addressof(Visitor.Values[Visitor.Values.size() - NumValues]);
      return std::span<Instruction *>(StartPtr, NumValues);
    }
    void reset() { Visitor.Values.clear(); }
  };
  ValueAccessView values() { return ValueAccessView(*this); }

  template <ranges::input_range T>
  void
  translate(T const &Instructions, BasicBlock *TransferTo, unsigned NumMerges) {
    bool Reachable = true;
    for (bytecode::Instruction const *Instruction : Instructions) {
      this->visit(Instruction);
      if (detail::isTerminatingInstruction(Instruction)) {
        Reachable = false;
        break;
      }
    }
    if (Reachable) {
      CurrentBasicBlock->BuildInst<minsts::Branch>(TransferTo);
      auto MergeValues = values().peek(NumMerges);
      detail::addMergeCandidates(TransferTo, MergeValues);
      values().pop(NumMerges);
    }
  }

  BasicBlock *getCurrentBasicBlock() const { return CurrentBasicBlock; }

  void operator()(binsts::Unreachable const *) {
    CurrentBasicBlock->BuildInst<minsts::Unreachable>();
    values().reset();
  }

  void operator()(binsts::Drop const *) { values().pop(); }

  void operator()(binsts::Block const *Inst) {
    auto BlockType = convertBlockResult(Inst->Type);
    auto NumParamTypes = BlockType.getParamTypes().size();
    auto NumResultTypes = BlockType.getResultTypes().size();
    auto *LandingBB = createBasicBlock();
    TranslationVisitor BodyVisitor(Context, CurrentBasicBlock, LandingBB);
    BodyVisitor.values().push(values().peek(NumParamTypes));
    values().pop(NumParamTypes);
    for (auto const &ResultValueType : BlockType.getResultTypes()) {
      auto *Phi = LandingBB->BuildInst<minsts::Phi>(ResultValueType);
      values().push(Phi);
    }
    labels().push(LandingBB, NumResultTypes);
    BodyVisitor.translate(Inst->Body, LandingBB, NumResultTypes);
    labels().pop();
    CurrentBasicBlock = LandingBB;
  }

  void operator()(binsts::Loop const *Inst) {
    auto BlockType = convertBlockResult(Inst->Type);
    auto NumParamTypes = BlockType.getParamTypes().size();
    auto NumResultTypes = BlockType.getResultTypes().size();
    auto *LoopBB = createBasicBlock();
    auto *LandingBB = createBasicBlock();
    TranslationVisitor BodyVisitor(Context, LoopBB, LandingBB);
    CurrentBasicBlock->BuildInst<minsts::Branch>(LoopBB);
    for (auto &&[LoopMergeValueType, PhiCandidate] : ranges::views::zip(
             BlockType.getResultTypes(), values().peek(NumParamTypes))) {
      auto *Phi = LoopBB->BuildInst<minsts::Phi>(LoopMergeValueType);
      Phi->addArgument(PhiCandidate);
      BodyVisitor.values().push(Phi);
    }
    values().pop(NumParamTypes);
    for (auto const &ResultValueType : BlockType.getResultTypes()) {
      auto *Phi = LandingBB->BuildInst<minsts::Phi>(ResultValueType);
      values().push(Phi);
    }
    labels().push(LoopBB, NumParamTypes);
    BodyVisitor.translate(Inst->Body, LandingBB, NumResultTypes);
    labels().pop();
    CurrentBasicBlock = LandingBB;
  }

  void operator()(binsts::If const *Inst) {
    auto *Condition = values().pop();
    auto BlockType = convertBlockResult(Inst->Type);
    auto NumParamTypes = BlockType.getParamTypes().size();
    auto NumResultTypes = BlockType.getResultTypes().size();
    auto *TrueBB = createBasicBlock();
    auto *FalseBB = createBasicBlock();
    auto *LandingBB = createBasicBlock();
    TranslationVisitor TrueVisitor(Context, TrueBB, FalseBB);
    TranslationVisitor FalseVisitor(Context, FalseBB, LandingBB);
    CurrentBasicBlock->BuildInst<minsts::Branch>(Condition, TrueBB, FalseBB);
    TrueVisitor.values().push(values().peek(NumParamTypes));
    FalseVisitor.values().push(values().peek(NumParamTypes));
    values().pop(NumParamTypes);
    for (auto const &MergeValueType : BlockType.getResultTypes()) {
      auto *Phi = LandingBB->BuildInst<minsts::Phi>(MergeValueType);
      values().push(Phi);
    }
    labels().push(LandingBB, NumResultTypes);
    TrueVisitor.translate(Inst->True, LandingBB, NumResultTypes);
    if (Inst->False.has_value())
      FalseVisitor.translate(*Inst->False, LandingBB, NumResultTypes);
    labels().pop();
    CurrentBasicBlock = LandingBB;
  }

#define CONSTANT(BYTECODE_INST, VALUE_TYPE)                                    \
  void operator()(BYTECODE_INST const *Inst) {                                 \
    auto Value = static_cast<VALUE_TYPE>(Inst->Value);                         \
    auto *Result = CurrentBasicBlock->BuildInst<minsts::Constant>(Value);      \
    values().push(Result);                                                     \
  }
  CONSTANT(binsts::I32Const, std::int32_t)
  CONSTANT(binsts::I64Const, std::int64_t)
  CONSTANT(binsts::F32Const, float)
  CONSTANT(binsts::F64Const, double)
#undef CONSTANT

#define INT_BINARY_OP(BYTECODE_INST, OPERATOR)                                 \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *RHS = values().pop();                                                \
    auto *LHS = values().pop();                                                \
    auto Operator = OPERATOR;                                                  \
    auto *Result =                                                             \
        CurrentBasicBlock->BuildInst<minsts::IntBinaryOp>(Operator, LHS, RHS); \
    values().push(Result);                                                     \
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

#define FP_BINARY_OP(BYTECODE_INST, OPERATOR)                                  \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *RHS = values().pop();                                                \
    auto *LHS = values().pop();                                                \
    auto Operator = OPERATOR;                                                  \
    auto *Result =                                                             \
        CurrentBasicBlock->BuildInst<minsts::FPBinaryOp>(Operator, LHS, RHS);  \
    values().push(Result);                                                     \
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

  template <bytecode::instruction T> void operator()(T const *) {
    SABLE_UNREACHABLE();
  }
};

TranslationTask::TranslationTask(
    EntityMap const &EntitiesResolver_,
    bytecode::views::Function SourceFunction_, mir::Function &TargetFunction_)
    : Context(std::make_unique<TranslationContext>(
          EntitiesResolver_, SourceFunction_, TargetFunction_)),
      Visitor(nullptr) {}

TranslationTask::TranslationTask(TranslationTask &&) noexcept = default;
TranslationTask::~TranslationTask() noexcept = default;

void TranslationTask::perform() {
  auto *EntryBB = Context->entry();
  auto *ExitBB = Context->exit();

  Visitor = std::make_unique<TranslationVisitor>(*Context, EntryBB, ExitBB);

  auto NumReturnValues = Context->source().getType()->getNumResult();
  Visitor->labels().push(ExitBB, NumReturnValues);
  Visitor->translate(*Context->source().getBody(), ExitBB, NumReturnValues);
  Visitor->labels().pop();
}
} // namespace mir::bytecode_codegen
