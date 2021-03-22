#include "IsWellformed.h"
#include "../../bytecode/Validation.h"
#include "Dominator.h"

#include <iterator>
#include <memory>

#include <range/v3/algorithm/binary_search.hpp>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/sort.hpp>

namespace mir::passes {

// clang-format off
void IsWellformedCallbackTrivial::hasNullOperand(ASTNode const*) 
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::hasInvalidType(ASTNode const*) 
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::hasInvalidImport(ImportableEntity const*) 
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::hasInvalidExport(ExportableEntity const*) 
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::referUnavailable(ASTNode const*) 
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::referNonDominating
(Instruction const*, Instruction const*) 
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::hasInvalidOperator(Instruction const*)
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::referNonDominatingPhi
(Instruction const*, Instruction const*, BasicBlock const*)
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::hasPhiAfterMerge(instructions::Phi const*)
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::appearAfterTerminatingInst(Instruction const*)
{ IsWellformed = false; }
void IsWellformedCallbackTrivial::missingTerminatingInst(BasicBlock const*)
{ IsWellformed = false; }
// clang-format on

IsWellformedModulePass::IsWellformedModulePass(
    std::shared_ptr<IsWellformedCallback> Callback_)
    : Callback(std::move(Callback_)) {}

// clang-format off
bool IsWellformedModulePass::has(mir::Global const &Global) const 
{ return ranges::binary_search(*AvailableNodes, std::addressof(Global)); }
bool IsWellformedModulePass::has(mir::Memory const &Memory) const 
{ return ranges::binary_search(*AvailableNodes, std::addressof(Memory)); }
bool IsWellformedModulePass::has(mir::Table const &Table) const 
{ return ranges::binary_search(*AvailableNodes, std::addressof(Table)); }
bool IsWellformedModulePass::has(mir::Function const &Function) const 
{ return ranges::binary_search(*AvailableNodes, std::addressof(Function)); }
bool IsWellformedModulePass::has(mir::DataSegment const &Data) const
{ return ranges::binary_search(*AvailableNodes, std::addressof(Data)); }
bool IsWellformedModulePass::has(mir::ElementSegment const &Element) const
{ return ranges::binary_search(*AvailableNodes, std::addressof(Element)); }
// clang-format on

struct IsWellformedModulePass::CheckInitializeExprVisitor :
    InitExprVisitorBase<CheckInitializeExprVisitor, void, false> {
  IsWellformedModulePass &ModulePass;

public:
  CheckInitializeExprVisitor(IsWellformedModulePass &ModulePass_)
      : ModulePass(ModulePass_) {}
  void operator()(initializer::Constant *) { return; }
  void operator()(initializer::GlobalGet *InitInst) {
    auto const *Target = InitInst->getGlobalValue();
    if (!Target) ModulePass.Callback->hasNullOperand(InitInst);
    if (Target && !ModulePass.has(*Target))
      ModulePass.Callback->referUnavailable(InitInst);
  }
};

void IsWellformedModulePass::checkInitializeExpr(InitializerExpr *Expr) {
  CheckInitializeExprVisitor Visitor(*this);
  Visitor.visit(Expr);
}

namespace {
namespace detail {
template <typename T>
class AddrBackInserter :
    public std::iterator<std::output_iterator_tag, typename T::value_type> {
  T *Container;

public:
  using container_type = T;
  AddrBackInserter() = default;
  AddrBackInserter(AddrBackInserter const &) = default;
  AddrBackInserter(AddrBackInserter &&) noexcept = default;
  AddrBackInserter &operator=(AddrBackInserter const &) = default;
  AddrBackInserter &operator=(AddrBackInserter &&) noexcept = default;
  ~AddrBackInserter() noexcept = default;
  explicit AddrBackInserter(T &Container_)
      : Container(std::addressof(Container_)) {}

  AddrBackInserter &operator*() { return *this; }
  AddrBackInserter &operator++() { return *this; }
  AddrBackInserter &operator++(int) { return *this; }
  template <typename U> AddrBackInserter &operator=(U &&Arg) {
    Container->push_back(std::addressof(Arg));
    return *this;
  }
};
} // namespace detail
} // namespace
void IsWellformedModulePass::prepare(mir::Module const &Module_) {
  Module = std::addressof(Module_);
  AvailableNodes = std::make_unique<std::vector<mir::ASTNode const *>>();
  detail::AddrBackInserter Iterator(*AvailableNodes);
  ranges::copy(Module->getMemories(), Iterator);
  ranges::copy(Module->getTables(), Iterator);
  ranges::copy(Module->getGlobals(), Iterator);
  ranges::copy(Module->getFunctions(), Iterator);
  ranges::copy(Module->getData(), Iterator);
  ranges::copy(Module->getElements(), Iterator);
  ranges::sort(*AvailableNodes);
}

PassStatus IsWellformedModulePass::run() {
  for (auto const &Memory : Module->getMemories()) {
    auto const *MemoryPtr = std::addressof(Memory);
    for (auto const *Initializer : Memory.getInitializers()) {
      if (!Initializer) Callback->hasNullOperand(MemoryPtr);
      if (Initializer && !has(*Initializer))
        Callback->referUnavailable(MemoryPtr);
      if (!bytecode::validation::validate(Memory.getType()))
        Callback->hasInvalidType(MemoryPtr);
    }
  }

  for (auto const &Table : Module->getTables()) {
    auto const *TablePtr = std::addressof(Table);
    for (auto const *Initializer : Table.getInitializers()) {
      if (!Initializer) Callback->hasNullOperand(TablePtr);
      if (Initializer && !has(*Initializer))
        Callback->referUnavailable(TablePtr);
      if (!bytecode::validation::validate(Table.getType()))
        Callback->hasInvalidType(TablePtr);
    }
  }

  for (auto const &Global : Module->getGlobals()) {
    auto const *GlobalPtr = std::addressof(Global);
    if (Global.isImported() && Global.hasInitializer())
      Callback->hasInvalidImport(GlobalPtr);
    if (Global.isExported() &&
        !(Global.isImported() || Global.hasInitializer()))
      Callback->hasInvalidExport(GlobalPtr);
    if (Global.hasInitializer()) checkInitializeExpr(Global.getInitializer());
    if (!bytecode::validation::validate(Global.getType()))
      Callback->hasInvalidType(GlobalPtr);
  }

  for (auto const &Function : Module->getFunctions()) {
    auto const *FunctionPtr = std::addressof(Function);
    if (Function.isImported() && Function.hasBody())
      Callback->hasInvalidImport(FunctionPtr);
    if (Function.isExported() && !(Function.isImported() || Function.hasBody()))
      Callback->hasInvalidExport(FunctionPtr);
    if (!bytecode::validation::validate(Function.getType()))
      Callback->hasInvalidType(FunctionPtr);
    if (!Function.isImported()) {
      SimpleFunctionPassDriver<IsWellformedFunctionPass> Driver(*this);
      Driver(Function);
    }
  }

  for (auto const &Data : Module->getData()) {
    auto const *DataPtr = std::addressof(Data);
    if (!DataPtr->getOffset()) {
      Callback->hasNullOperand(DataPtr);
    } else {
      checkInitializeExpr(Data.getOffset());
    }
  }

  for (auto const &Element : Module->getElements()) {
    auto const *ElementPtr = std::addressof(Element);
    if (!ElementPtr->getOffset()) {
      Callback->hasNullOperand(ElementPtr);
    } else {
      checkInitializeExpr(Element.getOffset());
    }
    for (auto const *FunctionPtr : Element.getContent()) {
      if (!FunctionPtr) Callback->hasNullOperand(ElementPtr);
      if (FunctionPtr && !has(*FunctionPtr))
        Callback->referUnavailable(ElementPtr);
    }
  }

  return PassStatus::Converged;
}

void IsWellformedModulePass::finalize() { AvailableNodes = nullptr; }

IsWellformedModulePass::AnalysisResult
IsWellformedModulePass::getResult() const {
  return *Callback;
}

IsWellformedFunctionPass::IsWellformedFunctionPass(
    IsWellformedModulePass const &ModulePass_)
    : ModulePass(std::addressof(ModulePass_)), Callback(ModulePass_.Callback) {}

void IsWellformedFunctionPass::prepare(mir::Function const &Function_) {
  Function = std::addressof(Function_);
  SimpleFunctionPassDriver<DominatorPass> Driver;
  Dominator = std::make_unique<DominatorPassResult>(Driver(Function_));
  AvailableBB = std::make_unique<std::vector<mir::BasicBlock const *>>();
  AvailableBB->reserve(Function->getNumBasicBlock());
  for (auto const &BasicBlock : Function->getBasicBlocks())
    AvailableBB->push_back(std::addressof(BasicBlock));
  ranges::sort(*AvailableBB);
  AvailableLocal = std::make_unique<std::vector<mir::Local const *>>();
  AvailableLocal->reserve(Function->getNumLocal());
  for (auto const &Local : Function->getLocals())
    AvailableLocal->push_back(std::addressof(Local));
  ranges::sort(*AvailableLocal);
}

struct IsWellformedFunctionPass::CheckInstVisitor :
    mir::InstVisitorBase<CheckInstVisitor, void> {
  IsWellformedFunctionPass &FunctionPass;

  bool isAvailableInst(
      mir::Instruction const *Inst, mir::Instruction const *Operand) {
    if (Inst->getParent() == Operand->getParent()) {
      auto const *Parent = Inst->getParent();
      auto SearchIter = ranges::find_if(
          Parent->begin(), Inst->getIterator(),
          [=](mir::Instruction const &Operand_) {
            return std::addressof(Operand_) == Operand;
          });
      if (SearchIter == Inst->getIterator()) return false;
    } else {
      auto const &OperandBB = *Operand->getParent();
      auto const &InstBB = *Inst->getParent();
      if (!FunctionPass.Dominator->dominate(OperandBB, InstBB)) return false;
    }
    return true;
  }

  bool
  isAvailablePhi(mir::Instruction const *Value, mir::BasicBlock const *Path) {
    auto const *ValueBB = Value->getParent();
    return FunctionPass.Dominator->dominate(*ValueBB, *Path);
  }

  IsWellformedCallback &callback() { return *FunctionPass.Callback; }
  // clang-format off
  bool has(mir::Global const &Global) const 
  { return FunctionPass.ModulePass->has(Global); }
  bool has(mir::Memory const &Memory) const 
  { return FunctionPass.ModulePass->has(Memory); }
  bool has(mir::Table const &Table) const 
  { return FunctionPass.ModulePass->has(Table); }
  bool has(mir::Function const &Function) const 
  { return FunctionPass.ModulePass->has(Function); }
  bool has(mir::DataSegment const &DataSegment) const 
  { return FunctionPass.ModulePass->has(DataSegment); }
  bool has(mir::ElementSegment const &ElementSegment) const 
  { return FunctionPass.ModulePass->has(ElementSegment); }
  bool has(mir::BasicBlock const &BasicBlock) const
  { return FunctionPass.has(BasicBlock); }
  bool has(mir::Local const &Local) const
  { return FunctionPass.has(Local); }
  // clang-format on

public:
  CheckInstVisitor(IsWellformedFunctionPass &FunctionPass_)
      : FunctionPass(FunctionPass_) {}

  void operator()(mir::instructions::Unreachable const *) { return; }

  void operator()(mir::instructions::Branch const *Inst) {
    if (!Inst->getTarget()) callback().hasNullOperand(Inst);

    if (Inst->getCondition() && !Inst->getFalseTarget())
      callback().hasNullOperand(Inst);
    if (Inst->getFalseTarget() && !Inst->getCondition())
      callback().hasNullOperand(Inst);

    if (Inst->getCondition() && !isAvailableInst(Inst, Inst->getCondition()))
      callback().referNonDominating(Inst, Inst->getCondition());
    if (Inst->getFalseTarget() && !has(*Inst->getFalseTarget()))
      callback().referUnavailable(Inst);
    if (Inst->getTarget() && !has(*Inst->getTarget()))
      callback().referUnavailable(Inst);
  }

  void operator()(mir::instructions::BranchTable const *Inst) {
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
    if (!Inst->getDefaultTarget()) callback().hasNullOperand(Inst);
    if (Inst->getDefaultTarget() && !has(*Inst->getDefaultTarget()))
      callback().referUnavailable(Inst);
    for (auto const *Target : Inst->getTargets()) {
      if (!Target) callback().hasNullOperand(Inst);
      if (Target && !has(*Target)) callback().referUnavailable(Inst);
    }
  }

  void operator()(mir::instructions::Return const *Inst) {
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
  }

  void operator()(mir::instructions::Call const *Inst) {
    if (!Inst->getTarget()) callback().hasNullOperand(Inst);
    if (Inst->getTarget() && !has(*Inst->getTarget()))
      callback().referUnavailable(Inst);
    for (auto const *Argument : Inst->getArguments()) {
      if (!Argument) callback().hasNullOperand(Inst);
      if (Argument && !isAvailableInst(Inst, Argument))
        callback().referNonDominating(Inst, Argument);
    }
  }

  void operator()(mir::instructions::CallIndirect const *Inst) {
    if (!Inst->getIndirectTable()) callback().hasNullOperand(Inst);
    if (Inst->getIndirectTable() && !has(*Inst->getIndirectTable()))
      callback().referUnavailable(Inst);
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
    if (!bytecode::validation::validate(Inst->getExpectType()))
      callback().hasInvalidType(Inst);
    for (auto const *Argument : Inst->getArguments()) {
      if (!Argument) callback().hasNullOperand(Inst);
      if (Argument && !isAvailableInst(Inst, Argument))
        callback().referNonDominating(Inst, Argument);
    }
  }

  void operator()(mir::instructions::Select const *Inst) {
    if (!Inst->getCondition()) callback().hasNullOperand(Inst);
    if (Inst->getCondition() && !isAvailableInst(Inst, Inst->getCondition()))
      callback().referNonDominating(Inst, Inst->getCondition());
    if (!Inst->getTrue()) callback().hasNullOperand(Inst);
    if (Inst->getTrue() && !isAvailableInst(Inst, Inst->getTrue()))
      callback().referNonDominating(Inst, Inst->getTrue());
    if (!Inst->getFalse()) callback().hasNullOperand(Inst);
    if (Inst->getFalse() && !isAvailableInst(Inst, Inst->getFalse()))
      callback().referNonDominating(Inst, Inst->getFalse());
  }

  void operator()(mir::instructions::LocalGet const *Inst) {
    if (!Inst->getTarget()) callback().hasNullOperand(Inst);
    if (Inst->getTarget() && !has(*Inst->getTarget()))
      callback().referUnavailable(Inst);
  }

  void operator()(mir::instructions::LocalSet const *Inst) {
    if (!Inst->getTarget()) callback().hasNullOperand(Inst);
    if (Inst->getTarget() && !has(*Inst->getTarget()))
      callback().referUnavailable(Inst);
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
  }

  void operator()(mir::instructions::GlobalGet const *Inst) {
    if (!Inst->getTarget()) callback().hasNullOperand(Inst);
    if (Inst->getTarget() && !has(*Inst->getTarget()))
      callback().referUnavailable(Inst);
  }

  void operator()(mir::instructions::GlobalSet const *Inst) {
    if (!Inst->getTarget()) callback().hasNullOperand(Inst);
    if (Inst->getTarget() && !has(*Inst->getTarget()))
      callback().referUnavailable(Inst);
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
  }

  void operator()(mir::instructions::Constant const *) { return; }

  bool validate(mir::instructions::IntUnaryOperator const &Operator) {
    using UnaryOperator = mir::instructions::IntUnaryOperator;
    switch (Operator) {
    case UnaryOperator::Eqz:
    case UnaryOperator::Clz:
    case UnaryOperator::Ctz:
    case UnaryOperator::Popcnt: return true;
    default: return false;
    }
  }

  void operator()(mir::instructions::IntUnaryOp const *Inst) {
    if (!validate(Inst->getOperator())) callback().hasInvalidOperator(Inst);
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
  }

  bool validate(mir::instructions::IntBinaryOperator const &Operator) {
    using BinaryOperator = mir::instructions::IntBinaryOperator;
    switch (Operator) {
    case BinaryOperator::Eq:
    case BinaryOperator::Ne:
    case BinaryOperator::LtS:
    case BinaryOperator::LtU:
    case BinaryOperator::GtS:
    case BinaryOperator::GtU:
    case BinaryOperator::LeS:
    case BinaryOperator::LeU:
    case BinaryOperator::GeS:
    case BinaryOperator::GeU:
    case BinaryOperator::Add:
    case BinaryOperator::Sub:
    case BinaryOperator::Mul:
    case BinaryOperator::DivS:
    case BinaryOperator::DivU:
    case BinaryOperator::RemS:
    case BinaryOperator::RemU:
    case BinaryOperator::And:
    case BinaryOperator::Or:
    case BinaryOperator::Xor:
    case BinaryOperator::Shl:
    case BinaryOperator::ShrS:
    case BinaryOperator::ShrU:
    case BinaryOperator::Rotl:
    case BinaryOperator::Rotr: return true;
    default: return false;
    }
  }

  void operator()(mir::instructions::IntBinaryOp const *Inst) {
    if (!validate(Inst->getOperator())) callback().hasInvalidOperator(Inst);
    if (!Inst->getLHS()) callback().hasNullOperand(Inst);
    if (!Inst->getRHS()) callback().hasNullOperand(Inst);
    if (Inst->getLHS() && !isAvailableInst(Inst, Inst->getLHS()))
      callback().referNonDominating(Inst, Inst->getLHS());
    if (Inst->getRHS() && !isAvailableInst(Inst, Inst->getRHS()))
      callback().referNonDominating(Inst, Inst->getRHS());
  }

  bool validate(mir::instructions::FPUnaryOperator const &Operator) {
    using UnaryOperator = mir::instructions::FPUnaryOperator;
    switch (Operator) {
    case UnaryOperator::Abs:
    case UnaryOperator::Neg:
    case UnaryOperator::Ceil:
    case UnaryOperator::Floor:
    case UnaryOperator::Trunc:
    case UnaryOperator::Nearest:
    case UnaryOperator::Sqrt: return true;
    default: return false;
    }
  }

  void operator()(mir::instructions::FPUnaryOp const *Inst) {
    if (!validate(Inst->getOperator())) callback().hasInvalidOperator(Inst);
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
  }

  bool validate(mir::instructions::FPBinaryOperator const &Operator) {
    using BinaryOperator = mir::instructions::FPBinaryOperator;
    switch (Operator) {
    case BinaryOperator::Eq:
    case BinaryOperator::Ne:
    case BinaryOperator::Lt:
    case BinaryOperator::Gt:
    case BinaryOperator::Le:
    case BinaryOperator::Ge:
    case BinaryOperator::Add:
    case BinaryOperator::Sub:
    case BinaryOperator::Mul:
    case BinaryOperator::Div:
    case BinaryOperator::Min:
    case BinaryOperator::Max:
    case BinaryOperator::CopySign: return true;
    default: return false;
    }
  }

  void operator()(mir::instructions::FPBinaryOp const *Inst) {
    if (!validate(Inst->getOperator())) callback().hasInvalidOperator(Inst);
    if (!Inst->getLHS()) callback().hasNullOperand(Inst);
    if (!Inst->getRHS()) callback().hasNullOperand(Inst);
    if (Inst->getLHS() && !isAvailableInst(Inst, Inst->getLHS()))
      callback().referNonDominating(Inst, Inst->getLHS());
    if (Inst->getRHS() && !isAvailableInst(Inst, Inst->getRHS()))
      callback().referNonDominating(Inst, Inst->getRHS());
  }

  void operator()(mir::instructions::Load const *Inst) {
    if (!Inst->getLinearMemory()) callback().hasNullOperand(Inst);
    if (Inst->getLinearMemory() && !has(*Inst->getLinearMemory()))
      callback().referUnavailable(Inst);
    if (!Inst->getAddress()) callback().hasNullOperand(Inst);
    if (Inst->getAddress() && !isAvailableInst(Inst, Inst->getAddress()))
      callback().referNonDominating(Inst, Inst->getAddress());
    if (!bytecode::validation::validate(Inst->getType()))
      callback().hasInvalidType(Inst);
  }

  void operator()(mir::instructions::Store const *Inst) {
    if (!Inst->getLinearMemory()) callback().hasNullOperand(Inst);
    if (Inst->getLinearMemory() && !has(*Inst->getLinearMemory()))
      callback().referUnavailable(Inst);
    if (!Inst->getAddress()) callback().hasNullOperand(Inst);
    if (Inst->getAddress() && !isAvailableInst(Inst, Inst->getAddress()))
      callback().referNonDominating(Inst, Inst->getAddress());
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
  }

  void operator()(mir::instructions::MemoryGuard const *Inst) {
    if (!Inst->getLinearMemory()) callback().hasNullOperand(Inst);
    if (Inst->getLinearMemory() && !has(*Inst->getLinearMemory()))
      callback().referUnavailable(Inst);
    if (!Inst->getAddress()) callback().hasNullOperand(Inst);
    if (Inst->getAddress() && !isAvailableInst(Inst, Inst->getAddress()))
      callback().referNonDominating(Inst, Inst->getAddress());
  }

  void operator()(mir::instructions::MemoryGrow const *Inst) {
    if (!Inst->getLinearMemory()) callback().hasNullOperand(Inst);
    if (Inst->getLinearMemory() && !has(*Inst->getLinearMemory()))
      callback().referUnavailable(Inst);
    if (!Inst->getSize()) callback().hasNullOperand(Inst);
    if (Inst->getSize() && !isAvailableInst(Inst, Inst->getSize()))
      callback().referNonDominating(Inst, Inst->getSize());
  }

  void operator()(mir::instructions::MemorySize const *Inst) {
    if (!Inst->getLinearMemory()) callback().hasNullOperand(Inst);
    if (Inst->getLinearMemory() && !has(*Inst->getLinearMemory()))
      callback().referUnavailable(Inst);
  }

  bool validate(mir::instructions::CastMode const &Mode) {
    using CastMode = mir::instructions::CastMode;
    switch (Mode) {
    case CastMode::Conversion:
    case CastMode::ConversionSigned:
    case CastMode::ConversionUnsigned:
    case CastMode::Reinterpret:
    case CastMode::SatConversionSigned:
    case CastMode::SatConversionUnsigned: return true;
    default: return false;
    }
  }

  void operator()(mir::instructions::Cast const *Inst) {
    if (!validate(Inst->getMode())) callback().hasInvalidOperator(Inst);
    if (!bytecode::validation::validate(Inst->getType()))
      callback().hasInvalidType(Inst);
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
  }

  void operator()(mir::instructions::Extend const *Inst) {
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
  }

  void operator()(mir::instructions::Pack const *Inst) {
    for (auto const *Argument : Inst->getArguments()) {
      if (!Argument) callback().hasNullOperand(Inst);
      if (Argument && !isAvailableInst(Inst, Argument))
        callback().referNonDominating(Inst, Argument);
    }
  }

  void operator()(mir::instructions::Unpack const *Inst) {
    if (!Inst->getOperand()) callback().hasNullOperand(Inst);
    if (Inst->getOperand() && !isAvailableInst(Inst, Inst->getOperand()))
      callback().referNonDominating(Inst, Inst->getOperand());
  }

  void operator()(mir::instructions::Phi const *Inst) {
    if (!bytecode::validation::validate(Inst->getType()))
      callback().hasInvalidType(Inst);
    for (auto const &[Value, Path] : Inst->getCandidates()) {
      if (!Value) callback().hasNullOperand(Inst);
      if (!Path) callback().hasNullOperand(Inst);
      if (Value && Path && !isAvailablePhi(Value, Path))
        callback().referNonDominatingPhi(Inst, Value, Path);
    }
  }
};

PassStatus IsWellformedFunctionPass::run() {
  CheckInstVisitor Visitor(*this);
  if (!bytecode::validation::validate(Function->getType()))
    Callback->hasInvalidType(Function);

  for (auto const &Local : Function->getLocals()) {
    auto const *LocalPtr = std::addressof(Local);
    if (!bytecode::validation::validate(LocalPtr->getType()))
      Callback->hasInvalidType(LocalPtr);
  }

  for (auto const &BasicBlock : Function->getBasicBlocks()) {
    bool HasNonPhi = false;
    bool HasTerminating = false;
    for (auto const &Instruction : BasicBlock) {
      auto const *InstPtr = std::addressof(Instruction);
      if (HasNonPhi && InstPtr->isPhi())
        Callback->hasPhiAfterMerge(dyn_cast<instructions::Phi>(InstPtr));
      if (HasTerminating) Callback->appearAfterTerminatingInst(InstPtr);
      if (InstPtr->isPhi()) HasNonPhi = true;
      if (InstPtr->isTerminating()) HasTerminating = true;
      Visitor.visit(InstPtr);
    }
    if (!HasTerminating)
      Callback->missingTerminatingInst(std::addressof(BasicBlock));
  }
  return PassStatus::Converged;
}

void IsWellformedFunctionPass::finalize() {
  Function = nullptr;
  Dominator = nullptr;
  AvailableBB = nullptr;
}

IsWellformedFunctionPass::AnalysisResult
IsWellformedFunctionPass::getResult() const {
  return *Callback;
}

bool IsWellformedFunctionPass::has(mir::BasicBlock const &BasicBlock) const {
  return ranges::binary_search(*AvailableBB, std::addressof(BasicBlock));
}

bool IsWellformedFunctionPass::has(mir::Local const &Local) const {
  return ranges::binary_search(*AvailableLocal, std::addressof(Local));
}
} // namespace mir::passes

namespace mir {
bool validate(mir::Module const &Module) {
  passes::SimpleModulePassDriver<passes::IsWellformedModulePass> Driver;
  auto const &Result =
      /* NOLINTNEXTLINE */
      static_cast<passes::IsWellformedCallbackTrivial const &>(Driver(Module));
  return Result.isWellformed();
}
} // namespace mir
