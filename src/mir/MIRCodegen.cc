#include "MIRCodegen.h"

#include <range/v3/view/transform.hpp>

namespace mir::bytecode_codegen {
using namespace bytecode::valuetypes;
namespace minsts = mir::instructions;
namespace binsts = bytecode::instructions;
///////////////////////////// EntityLayout /////////////////////////////////////
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

template <ranges::random_access_range T, typename IndexType>
typename T::value_type getFromContainer(T const &Container, IndexType Index) {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Container.size());
  return Container[CastedIndex];
}
} // namespace detail
} // namespace

// TODO: in the future, we need a better strategy handling constant offset
//       initializer expression. Currently, due to WebAssembly validation,
//       Constant expression, must be a constant or a GlobalGet instruction.
std::unique_ptr<ConstantExpr>
EntityLayout::solveInitializerExpr(bytecode::Expression const &Expr) {
  assert(Expr.size() == 1);
  auto const &Inst = Expr.front();
  if (is_a<binsts::I32Const>(Inst)) {
    auto const *CastedPtr = dyn_cast<binsts::I32Const>(Inst);
    return std::make_unique<constant_expr::Constant>(CastedPtr->Value);
  }
  if (is_a<binsts::I64Const>(Inst)) {
    auto const *CastedPtr = dyn_cast<binsts::I64Const>(Inst);
    return std::make_unique<constant_expr::Constant>(CastedPtr->Value);
  }
  if (is_a<binsts::F32Const>(Inst)) {
    auto const *CastedPtr = dyn_cast<binsts::F32Const>(Inst);
    return std::make_unique<constant_expr::Constant>(CastedPtr->Value);
  }
  if (is_a<binsts::F64Const>(Inst)) {
    auto const *CastedPtr = dyn_cast<binsts::F64Const>(Inst);
    return std::make_unique<constant_expr::Constant>(CastedPtr->Value);
  }
  if (is_a<binsts::GlobalGet>(Inst)) {
    auto const *CastedPtr = dyn_cast<binsts::GlobalGet>(Inst);
    auto *Target = this->operator[](CastedPtr->Target);
    return std::make_unique<constant_expr::GlobalGet>(Target);
  }
  SABLE_UNREACHABLE();
}

EntityLayout::EntityLayout(bytecode::Module const &BModule, Module &MModule)
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
    if (BGlobal.isDefinition())
      MGlobal->setInitializer(solveInitializerExpr(*BGlobal.getInitializer()));
    Globals.push_back(MGlobal);
  }
  for (auto const &BFunction : BModuleView.functions()) {
    auto *MFunction = MModule.BuildFunction(*BFunction.getType());
    detail::setImportExportInfo(MFunction, BFunction);
    Functions.push_back(MFunction);
  }
  for (auto const &BDataSegment : BModuleView.module().Data) {
    auto Offset = solveInitializerExpr(BDataSegment.Offset);
    auto *Target = this->operator[](BDataSegment.Memory);
    auto *MDataSegment =
        MModule.BuildDataSegment(std::move(Offset), BDataSegment.Initializer);
    Target->addInitializer(MDataSegment);
  }
  for (auto const &BElementSegment : BModuleView.module().Elements) {
    auto Offset = solveInitializerExpr(BElementSegment.Offset);
    auto *Target = this->operator[](BElementSegment.Table);
    // clang-format off
    auto Content =
        BElementSegment.Initializer
        | ranges::views::transform([this](bytecode::FuncIDX const &Index) {
            return this->operator[](Index);
          })
        | ranges::to<std::vector<Function *>>();
    // clang-format on
    auto *MElementSegment =
        MModule.BuildElementSegment(std::move(Offset), Content);
    Target->addInitializer(MElementSegment);
  }
}

Function *EntityLayout::operator[](bytecode::FuncIDX Index) const {
  return detail::getFromContainer(Functions, Index);
}

Memory *EntityLayout::operator[](bytecode::MemIDX Index) const {
  return detail::getFromContainer(Memories, Index);
}

Table *EntityLayout::operator[](bytecode::TableIDX Index) const {
  return detail::getFromContainer(Tables, Index);
}

Global *EntityLayout::operator[](bytecode::GlobalIDX Index) const {
  return detail::getFromContainer(Globals, Index);
}

bytecode::FunctionType const *
EntityLayout::operator[](bytecode::TypeIDX Index) const {
  return BModuleView[Index];
}

Memory *EntityLayout::getImplicitMemory() const {
  assert(!Memories.empty());
  return Memories.front();
}

Table *EntityLayout::getImplicitTable() const {
  assert(!Tables.empty());
  return Tables.front();
}

////////////////////////// FunctionTranslationTask /////////////////////////////
namespace {
namespace detail {
template <ranges::input_range T>
void addMergeCandidates(
    BasicBlock *MergeBlock, BasicBlock *From, T const &PhiCandidates) {
  auto MergeBlockView =
      ranges::views::all(*MergeBlock) |
      ranges::views::transform([](auto &&X) { return std::addressof(X); });
  auto PhiCandidatesView =
      PhiCandidates |
      ranges::views::transform([](auto &&X) { return std::addressof(X); });
  for (auto &&[Instruction, CandidateValue] :
       ranges::views::zip(MergeBlockView, PhiCandidatesView)) {
    auto *PhiNode = dyn_cast<minsts::Phi>(Instruction);
    PhiNode->addCandidate(*CandidateValue, From);
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

class FunctionTranslationTask::TranslationContext {
  EntityLayout const &E;
  bytecode::views::Function SourceFunction;
  mir::Function &TargetFunction;
  std::vector<mir::Local *> Locals;
  BasicBlock *EntryBasicBlock;
  BasicBlock *ExitBasicBlock;

  // < Branch Target with Merge Phi Nodes, Num of Phi Nodes>
  std::vector<std::tuple<BasicBlock *, unsigned>> Labels;

public:
  TranslationContext(
      EntityLayout const &EntityLayout_,
      bytecode::views::Function SourceFunction_, mir::Function &TargetFunction_)
      : E(EntityLayout_), SourceFunction(SourceFunction_),
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
  bool empty() const { return Labels.empty(); }

  Memory *getImplicitMemory() const { return E.getImplicitMemory(); }
  Table *getImplicitTable() const { return E.getImplicitTable(); }
};

class FunctionTranslationTask::TranslationVisitor :
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
    bool empty() const { return Visitor.Context.empty(); }
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
      // Fixing a undefined behavior when NumValues == 0
      // StartPtr points to a possible null value which takes as reference later
      if (NumValues == 0) return std::span<Instruction *, 0>();
      auto *StartPtr =
          std::addressof(Visitor.Values[Visitor.Values.size() - NumValues]);
      return std::span<Instruction *>(StartPtr, NumValues);
    }
    void clear() { Visitor.Values.clear(); }
    bool empty() const { return Visitor.Values.empty(); }
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
      detail::addMergeCandidates(TransferTo, CurrentBasicBlock, MergeValues);
      values().pop(NumMerges);
      // expect all values to be consumed, ensured by validation hopefully
      assert(values().empty());
    }
  }

  BasicBlock *getCurrentBasicBlock() const { return CurrentBasicBlock; }

  void operator()(binsts::Unreachable const *) {
    CurrentBasicBlock->BuildInst<minsts::Unreachable>();
    values().clear();
  }

  void operator()(binsts::Nop const *) {} // IGNORED

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
      Phi->addCandidate(PhiCandidate, CurrentBasicBlock);
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
    if (Inst->False.has_value()) {
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
      FalseVisitor.translate(*Inst->False, LandingBB, NumResultTypes);
      labels().pop();
      CurrentBasicBlock = LandingBB;
    } else {
      assert((NumParamTypes == 0) && (NumResultTypes == 0));
      auto *TrueBB = createBasicBlock();
      auto *LandingBB = createBasicBlock();
      TranslationVisitor TrueVisitor(Context, TrueBB, LandingBB);
      CurrentBasicBlock->BuildInst<minsts::Branch>(
          Condition, TrueBB, LandingBB);
      labels().push(LandingBB, NumResultTypes);
      TrueVisitor.translate(Inst->True, LandingBB, NumResultTypes);
      labels().pop();
      CurrentBasicBlock = LandingBB;
    }
  }

  void operator()(binsts::Br const *Inst) {
    auto [TargetBB, NumPhiNodes] = Context[Inst->Target];
    auto PhiCandidates = values().peek(NumPhiNodes);
    detail::addMergeCandidates(TargetBB, CurrentBasicBlock, PhiCandidates);
    CurrentBasicBlock->BuildInst<minsts::Branch>(TargetBB);
    values().clear();
  }

  void operator()(binsts::BrIf const *Inst) {
    auto [TargetBB, NumPhiNodes] = Context[Inst->Target];
    auto *Condition = values().pop();
    auto PhiCandidates = values().peek(NumPhiNodes);
    auto *FalseBB = createBasicBlock();
    detail::addMergeCandidates(TargetBB, CurrentBasicBlock, PhiCandidates);
    CurrentBasicBlock->BuildInst<minsts::Branch>(Condition, TargetBB, FalseBB);
    CurrentBasicBlock = FalseBB;
  }

  void operator()(binsts::BrTable const *Inst) {
    auto *Operand = values().pop();
    auto [DefaultBB, DefaultNumPhiNodes] = Context[Inst->DefaultTarget];
    auto PhiCandidates = values().peek(DefaultNumPhiNodes);
    detail::addMergeCandidates(DefaultBB, CurrentBasicBlock, PhiCandidates);
    std::vector<BasicBlock *> Targets;
    Targets.reserve(Inst->Targets.size());
    for (auto TargetIndex : Inst->Targets) {
      auto [TargetBB, TargetNumPhiNodes] = Context[TargetIndex];
      Targets.push_back(TargetBB);
      assert(TargetNumPhiNodes == DefaultNumPhiNodes);
      utility::ignore(TargetNumPhiNodes);
      detail::addMergeCandidates(TargetBB, CurrentBasicBlock, PhiCandidates);
    }
    CurrentBasicBlock->BuildInst<minsts::BranchTable>(
        Operand, DefaultBB, Targets);
    values().clear();
  }

  void operator()(binsts::Return const *) {
    CurrentBasicBlock->BuildInst<minsts::Branch>(Context.exit());
    auto NumReturnValues = Context.source().getType()->getResultTypes().size();
    auto ReturnValues = values().peek(NumReturnValues);
    detail::addMergeCandidates(Context.exit(), CurrentBasicBlock, ReturnValues);
    values().clear();
  }

  void operator()(binsts::Call const *Inst) {
    auto *Target = Context[Inst->Target];
    auto NumArguments = Target->getType().getParamTypes().size();
    auto Arguments =
        ranges::to<std::vector<Instruction *>>(values().peek(NumArguments));
    values().pop(NumArguments);
    auto *Result =
        CurrentBasicBlock->BuildInst<minsts::Call>(Target, Arguments);
    if (Target->getType().isVoidResult()) {} // Do nothing
    if (Target->getType().isSingleValueResult()) values().push(Result);
    if (Target->getType().isMultiValueResult()) {
      for (std::size_t I = 0; I < Target->getType().getNumResult(); ++I) {
        auto *UnpackResult =
            CurrentBasicBlock->BuildInst<minsts::Unpack>(Result, I);
        values().push(UnpackResult);
      }
    }
  }

  void operator()(binsts::CallIndirect const *Inst) {
    auto const *Type = Context[Inst->Type];
    auto *Table = Context.getImplicitTable();
    auto *Operand = values().pop();
    auto NumArguments = Type->getParamTypes().size();
    auto Arguments =
        ranges::to<std::vector<Instruction *>>(values().peek(NumArguments));
    values().pop(NumArguments);
    auto *Result = CurrentBasicBlock->BuildInst<minsts::CallIndirect>(
        Table, Operand, *Type, Arguments);
    if (Type->isVoidResult()) {} // Do nothing
    if (Type->isSingleValueResult()) values().push(Result);
    if (Type->isMultiValueResult()) {
      for (std::size_t I = 0; I < Type->getNumResult(); ++I) {
        auto *UnpackResult =
            CurrentBasicBlock->BuildInst<minsts::Unpack>(Result, I);
        values().push(UnpackResult);
      }
    }
  }

  void operator()(binsts::Drop const *) { utility::ignore(values().pop()); }

  void operator()(binsts::Select const *) {
    auto *Condition = values().pop();
    auto *FalseValue = values().pop();
    auto *TrueValue = values().pop();
    auto *Result = CurrentBasicBlock->BuildInst<minsts::Select>(
        Condition, TrueValue, FalseValue);
    values().push(Result);
  }

  void operator()(binsts::LocalGet const *Inst) {
    auto *Target = Context[Inst->Target];
    auto *Result = CurrentBasicBlock->BuildInst<minsts::LocalGet>(Target);
    values().push(Result);
  }

  void operator()(binsts::LocalSet const *Inst) {
    auto *Operand = values().pop();
    auto *Target = Context[Inst->Target];
    CurrentBasicBlock->BuildInst<minsts::LocalSet>(Target, Operand);
  }

  void operator()(binsts::LocalTee const *Inst) {
    auto *Operand = values().pop();
    auto *Target = Context[Inst->Target];
    CurrentBasicBlock->BuildInst<minsts::LocalSet>(Target, Operand);
    values().push(Operand);
  }

  void operator()(binsts::GlobalGet const *Inst) {
    auto *Target = Context[Inst->Target];
    auto *Result = CurrentBasicBlock->BuildInst<minsts::GlobalGet>(Target);
    values().push(Result);
  }

  void operator()(binsts::GlobalSet const *Inst) {
    auto *Operand = values().pop();
    auto *Target = Context[Inst->Target];
    CurrentBasicBlock->BuildInst<minsts::GlobalSet>(Target, Operand);
  }

#define LOAD_ZERO_EXTEND(BYTECODE_INST, LOAD_TYPE, LOAD_WIDTH)                 \
  void operator()(BYTECODE_INST const *Inst) {                                 \
    auto LoadType = LOAD_TYPE;                                                 \
    auto LoadWidth = LOAD_WIDTH;                                               \
    auto *Mem = Context.getImplicitMemory();                                   \
    auto *Address = values().pop();                                            \
    if (Inst->Offset != 0) {                                                   \
      auto *Offset = CurrentBasicBlock->BuildInst<minsts::Constant>(           \
          static_cast<std::int32_t>(Inst->Offset));                            \
      Address = CurrentBasicBlock->BuildInst<minsts::IntBinaryOp>(             \
          minsts::IntBinaryOperator::Add, Address, Offset);                    \
    }                                                                          \
    CurrentBasicBlock->BuildInst<minsts::MemoryGuard>(                         \
        Mem, Address, LoadWidth);                                              \
    auto *Result = CurrentBasicBlock->BuildInst<minsts::Load>(                 \
        Mem, LoadType, Address, LoadWidth);                                    \
    values().push(Result);                                                     \
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
  void operator()(BYTECODE_INST const *Inst) {                                 \
    auto LoadType = LOAD_TYPE;                                                 \
    auto LoadWidth = LOAD_WIDTH;                                               \
    auto *Mem = Context.getImplicitMemory();                                   \
    auto *Address = values().pop();                                            \
    if (Inst->Offset != 0) {                                                   \
      auto *Offset = CurrentBasicBlock->BuildInst<minsts::Constant>(           \
          static_cast<std::int32_t>(Inst->Offset));                            \
      Address = CurrentBasicBlock->BuildInst<minsts::IntBinaryOp>(             \
          minsts::IntBinaryOperator::Add, Address, Offset);                    \
    }                                                                          \
    CurrentBasicBlock->BuildInst<minsts::MemoryGuard>(                         \
        Mem, Address, LoadWidth);                                              \
    auto *Result = CurrentBasicBlock->BuildInst<minsts::Load>(                 \
        Mem, LoadType, Address, LoadWidth);                                    \
    auto *ExtendedResult =                                                     \
        CurrentBasicBlock->BuildInst<minsts::Extend>(Result, LoadWidth);       \
    values().push(ExtendedResult);                                             \
  }
  // clang-format off
  LOAD_SIGN_EXTEND(binsts::I32Load8S , I32, 8 )
  LOAD_SIGN_EXTEND(binsts::I32Load16S, I32, 16)
  LOAD_SIGN_EXTEND(binsts::I64Load8S , I64, 8 )
  LOAD_SIGN_EXTEND(binsts::I64Load16S, I64, 16)
  LOAD_SIGN_EXTEND(binsts::I64Load32S, I64, 32)
  // clang-format on
#undef LOAD_SIGN_EXTEND

#define STORE(BYTECODE_INST, STORE_WIDTH)                                      \
  void operator()(BYTECODE_INST const *Inst) {                                 \
    auto StoreWidth = STORE_WIDTH;                                             \
    auto *Mem = Context.getImplicitMemory();                                   \
    auto *Operand = values().pop();                                            \
    auto *Address = values().pop();                                            \
    if (Inst->Offset != 0) {                                                   \
      auto *Offset = CurrentBasicBlock->BuildInst<minsts::Constant>(           \
          static_cast<std::int32_t>(Inst->Offset));                            \
      Address = CurrentBasicBlock->BuildInst<minsts::IntBinaryOp>(             \
          minsts::IntBinaryOperator::Add, Address, Offset);                    \
    }                                                                          \
    CurrentBasicBlock->BuildInst<minsts::MemoryGuard>(                         \
        Mem, Address, StoreWidth);                                             \
    CurrentBasicBlock->BuildInst<minsts::Store>(                               \
        Mem, Address, Operand, StoreWidth);                                    \
  }
  // clang-format off
  STORE(binsts::I32Store  , 32)
  STORE(binsts::I64Store  , 64)
  STORE(binsts::F32Store  , 32)
  STORE(binsts::F64Store  , 64)
  STORE(binsts::I32Store8 , 8 )
  STORE(binsts::I32Store16, 16)
  STORE(binsts::I64Store8 , 8 )
  STORE(binsts::I64Store16, 16)
  STORE(binsts::I64Store32, 32)
  // clang-format on
#undef STORE

  void operator()(binsts::MemorySize const *) {
    auto *Mem = Context.getImplicitMemory();
    std::array<ASTNode *, 1> Operands{Mem};
    auto *Result = CurrentBasicBlock->BuildInst<minsts::Intrinsic>(
        minsts::IntrinsicFunction::MemorySize, Operands);
    values().push(Result);
  }

  void operator()(binsts::MemoryGrow const *) {
    auto *Operand = values().pop();
    auto *Mem = Context.getImplicitMemory();
    std::array<ASTNode *, 2> Operands{Mem, Operand};
    auto *Result = CurrentBasicBlock->BuildInst<minsts::Intrinsic>(
        minsts::IntrinsicFunction::MemoryGrow, Operands);
    values().push(Result);
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

#define INT_UNARY_OP(BYTECODE_INST, OPERATOR)                                  \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *Operand = values().pop();                                            \
    auto Operator = OPERATOR;                                                  \
    auto *Result =                                                             \
        CurrentBasicBlock->BuildInst<minsts::IntUnaryOp>(Operator, Operand);   \
    values().push(Result);                                                     \
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

#define FP_UNARY_OP(BYTECODE_INST, OPERATOR)                                   \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *Operand = values().pop();                                            \
    auto Operator = OPERATOR;                                                  \
    auto *Result =                                                             \
        CurrentBasicBlock->BuildInst<minsts::FPUnaryOp>(Operator, Operand);    \
    values().push(Result);                                                     \
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

#define CAST(BYTECODE_INST, CAST_MODE, TARGET_TYPE)                            \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *Operand = values().pop();                                            \
    auto ConvertMode = CAST_MODE;                                              \
    auto TargetType = TARGET_TYPE;                                             \
    auto *Result = CurrentBasicBlock->BuildInst<minsts::Cast>(                 \
        ConvertMode, TargetType, Operand);                                     \
    values().push(Result);                                                     \
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

#define EXTEND(BYTECODE_INST, FROM_WIDTH)                                      \
  void operator()(BYTECODE_INST const *) {                                     \
    auto *Operand = values().pop();                                            \
    auto FromWidth = FROM_WIDTH;                                               \
    auto *Result =                                                             \
        CurrentBasicBlock->BuildInst<minsts::Extend>(Operand, FromWidth);      \
    values().push(Result);                                                     \
  }
  // clang-format off
  EXTEND(binsts::I32Extend8S , 8 )
  EXTEND(binsts::I32Extend16S, 16)
  EXTEND(binsts::I64Extend8S , 8 )
  EXTEND(binsts::I64Extend16S, 16)
  EXTEND(binsts::I64Extend32S, 32)
  // clang-format on
#undef EXTEND
};

FunctionTranslationTask::FunctionTranslationTask(
    EntityLayout const &EntityLayout_,
    bytecode::views::Function SourceFunction_, mir::Function &TargetFunction_)
    : Context(std::make_unique<TranslationContext>(
          EntityLayout_, SourceFunction_, TargetFunction_)) {}

FunctionTranslationTask::FunctionTranslationTask(
    FunctionTranslationTask &&) noexcept = default;
FunctionTranslationTask &FunctionTranslationTask::operator=(
    FunctionTranslationTask &&) noexcept = default;
FunctionTranslationTask::~FunctionTranslationTask() noexcept = default;

void FunctionTranslationTask::perform() {
  auto *EntryBB = Context->entry();
  auto *ExitBB = Context->exit();

  TranslationVisitor Visitor(*Context, EntryBB, ExitBB);

  auto NumReturnValues = Context->source().getType()->getNumResult();
  Visitor.labels().push(ExitBB, NumReturnValues);
  Visitor.translate(*Context->source().getBody(), ExitBB, NumReturnValues);
  Visitor.labels().pop();
  // expect all labels are consumed, ensured by validation hopefully
  assert(Visitor.labels().empty());
}

/////////////////////////// ModuleTranslationTask //////////////////////////////
ModuleTranslationTask::ModuleTranslationTask(
    bytecode::Module const &Source_, mir::Module &Target_)
    : LayOut(nullptr), Source(std::addressof(Source_)),
      Target(std::addressof(Target_)), Names(nullptr) {}

ModuleTranslationTask::ModuleTranslationTask(
    bytecode::Module const &Source_, mir::Module &Target_,
    parser::customsections::Name const &Names_)
    : LayOut(nullptr), Source(std::addressof(Source_)),
      Target(std::addressof(Target_)), Names(std::addressof(Names_)) {}

void ModuleTranslationTask::perform() {
  LayOut = std::make_unique<EntityLayout>(*Source, *Target);
  for (auto const &[SourceFunc, TargetFunc] : LayOut->functions()) {
    if (SourceFunc.isDeclaration()) continue;
    FunctionTranslationTask FTask(*LayOut, SourceFunc, *TargetFunc);
    FTask.perform();
  }
  if (Names != nullptr) {
    for (auto const &FuncNameEntry : Names->getFunctionNames()) {
      auto *MFunction = (*LayOut)[FuncNameEntry.FuncIndex];
      MFunction->setName(FuncNameEntry.Name);
    }
    for (auto const &LocalNameEntry : Names->getLocalNames()) {
      auto *MFunction = (*LayOut)[LocalNameEntry.FuncIndex];
      auto &MLocal = *std::next(
          MFunction->local_begin(),
          static_cast<std::size_t>(LocalNameEntry.LocalIndex));
      MLocal.setName(LocalNameEntry.Name);
    }
  }
}

} // namespace mir::bytecode_codegen
