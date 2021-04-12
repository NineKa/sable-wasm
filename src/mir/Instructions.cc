#include "ASTNode.h"
#include "BasicBlock.h"
#include "Function.h"
#include "Instruction.h"
#include "Module.h"

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/replace.hpp>

namespace mir::instructions {
using IKind = InstructionKind;
///////////////////////////////// Unreachable //////////////////////////////////
Unreachable::Unreachable() : Instruction(IKind::Unreachable) {}

void Unreachable::replace(ASTNode const *, ASTNode *) noexcept {
  utility::unreachable();
}

bool Unreachable::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Unreachable;
}

bool Unreachable::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Unreachable::classof(dyn_cast<Instruction>(Node));
  return false;
}

//////////////////////////////////// Branch ////////////////////////////////////
Branch::Branch(BranchKind Kind_)
    : Instruction(InstructionKind::Branch), Kind(Kind_) {}
Branch::~Branch() noexcept = default;

BranchKind Branch::getBranchKind() const { return Kind; }

bool Branch::isConditional() const { return Kind == BranchKind::Conditional; }
bool Branch::isSwitch() const { return Kind == BranchKind::Switch; }
bool Branch::isUnconditional() const {
  return Kind == BranchKind::Unconditional;
}

branch::Unconditional &Branch::asUnconditional() {
  assert(isUnconditional());
  return static_cast<branch::Unconditional &>(*this);
}

branch::Conditional &Branch::asConditional() {
  assert(isConditional());
  return static_cast<branch::Conditional &>(*this);
}

branch::Switch &Branch::asSwitch() {
  assert(isSwitch());
  return static_cast<branch::Switch &>(*this);
}

branch::Unconditional const &Branch::asUnconditional() const {
  assert(isUnconditional());
  return static_cast<branch::Unconditional const &>(*this);
}

branch::Conditional const &Branch::asConditional() const {
  assert(isConditional());
  return static_cast<branch::Conditional const &>(*this);
}

branch::Switch const &Branch::asSwitch() const {
  assert(isSwitch());
  return static_cast<branch::Switch const &>(*this);
}

bool Branch::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == InstructionKind::Branch;
}

bool Branch::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Branch::classof(dyn_cast<Instruction>(Node));
  return false;
}

namespace branch {
Unconditional::Unconditional(mir::BasicBlock *Target_)
    : Branch(BranchKind::Unconditional), Target() {
  setTarget(Target_);
}

Unconditional::~Unconditional() noexcept {
  if (Target != nullptr) Target->remove_use(this);
}

mir::BasicBlock *Unconditional::getTarget() const { return Target; }

void Unconditional::setTarget(mir::BasicBlock *Target_) {
  if (Target != nullptr) Target->remove_use(this);
  if (Target_ != nullptr) Target_->add_use(this);
  Target = Target_;
}

void Unconditional::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getTarget() == Old) setTarget(dyn_cast<mir::BasicBlock>(New));
}

bool Unconditional::classof(Branch const *Inst) {
  return Inst->isUnconditional();
}

bool Unconditional::classof(Instruction const *Inst) {
  if (Branch::classof(Inst))
    return Unconditional::classof(dyn_cast<Branch>(Inst));
  return false;
}

bool Unconditional::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Unconditional::classof(dyn_cast<Instruction>(Node));
  return false;
}

Conditional::Conditional(
    mir::Instruction *Operand_, mir::BasicBlock *True_, mir::BasicBlock *False_)
    : Branch(BranchKind::Conditional), Operand(), True(), False() {
  setOperand(Operand_);
  setTrue(True_);
  setFalse(False_);
}

Conditional::~Conditional() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
  if (True != nullptr) True->remove_use(this);
  if (False != nullptr) False->remove_use(this);
}

mir::Instruction *Conditional::getOperand() const { return Operand; }
mir::BasicBlock *Conditional::getTrue() const { return True; }
mir::BasicBlock *Conditional::getFalse() const { return False; }

void Conditional::setOperand(mir::Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void Conditional::setTrue(mir::BasicBlock *True_) {
  if (True != nullptr) True->remove_use(this);
  if (True_ != nullptr) True_->add_use(this);
  True = True_;
}

void Conditional::setFalse(mir::BasicBlock *False_) {
  if (False != nullptr) False->remove(this);
  if (False_ != nullptr) False_->add_use(this);
  False = False_;
}

void Conditional::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
  if (getTrue() == Old) setTrue(dyn_cast<BasicBlock>(New));
  if (getFalse() == Old) setFalse(dyn_cast<BasicBlock>(New));
}

bool Conditional::classof(Branch const *Inst) { return Inst->isConditional(); }

bool Conditional::classof(Instruction const *Inst) {
  if (Branch::classof(Inst))
    return Conditional::classof(dyn_cast<Branch>(Inst));
  return false;
}

bool Conditional::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Conditional::classof(dyn_cast<Instruction>(Node));
  return false;
}

Switch::Switch(
    mir::Instruction *Operand_, mir::BasicBlock *DefaultTarget_,
    std::span<mir::BasicBlock *const> Targets_)
    : Branch(BranchKind::Switch), Operand(), DefaultTarget(), Targets() {
  setOperand(Operand_);
  setDefaultTarget(DefaultTarget_);
  setTargets(Targets_);
}

Switch::~Switch() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
  if (DefaultTarget != nullptr) DefaultTarget->remove_use(this);
  for (auto *Target : Targets)
    if (Target != nullptr) Target->remove_use(this);
}

mir::Instruction *Switch::getOperand() const { return Operand; }
mir::BasicBlock *Switch::getDefaultTarget() const { return DefaultTarget; }

void Switch::setOperand(mir::Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void Switch::setDefaultTarget(mir::BasicBlock *DefaultTarget_) {
  if (DefaultTarget != nullptr) DefaultTarget->remove_use(this);
  if (DefaultTarget_ != nullptr) DefaultTarget_->add_use(this);
  DefaultTarget = DefaultTarget_;
}

std::size_t Switch::getNumTargets() const { return Targets.size(); }

mir::BasicBlock *Switch::getTarget(std::size_t Index) const {
  assert(Index < getNumTargets());
  return Targets[Index];
}

void Switch::setTarget(std::size_t Index, mir::BasicBlock *Target_) {
  assert(Index < getNumTargets());
  if (Targets[Index] != nullptr) Targets[Index]->remove_use(this);
  if (Target_ != nullptr) Target_->add_use(this);
  Targets[Index] = Target_;
}

void Switch::setTargets(std::span<mir::BasicBlock *const> Targets_) {
  for (auto *Target : Targets)
    if (Target != nullptr) Target->remove_use(this);
  for (auto *Target : Targets_)
    if (Target != nullptr) Target->add_use(this);
  Targets = Targets_ | ranges::to<std::vector<mir::BasicBlock *>>();
}

void Switch::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
  if (getDefaultTarget() == Old) setDefaultTarget(dyn_cast<BasicBlock>(New));
  for (std::size_t I = 0; I < getNumTargets(); ++I)
    if (getTarget(I) == Old) setTarget(I, dyn_cast<BasicBlock>(New));
}

bool Switch::classof(Branch const *Inst) { return Inst->isSwitch(); }

bool Switch::classof(Instruction const *Inst) {
  if (Branch::classof(Inst)) return Switch::classof(dyn_cast<Branch>(Inst));
  return false;
}

bool Switch::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Switch::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace branch

/////////////////////////////////// Return /////////////////////////////////////
Return::Return() : Instruction(IKind::Return), Operand() {}

Return::Return(Instruction *Operand_) : Instruction(IKind::Return), Operand() {
  setOperand(Operand_);
}

Return::~Return() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

bool Return::hasReturnValue() const { return Operand != nullptr; }
Instruction *Return::getOperand() const { return Operand; }

void Return::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void Return::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool Return::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Return;
}

bool Return::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Return::classof(dyn_cast<Instruction>(Node));
  return false;
}

//////////////////////////////////// Call //////////////////////////////////////
Call::Call(Function *Target_, std::span<Instruction *const> Arguments_)
    : Instruction(IKind::Call), Target(), Arguments() {
  setTarget(Target_);
  setArguments(Arguments_);
}

Call::~Call() noexcept {
  if (Target != nullptr) Target->remove_use(this);
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
}

Function *Call::getTarget() const { return Target; }
std::span<Instruction *const> Call::getArguments() const { return Arguments; }

void Call::setTarget(Function *Target_) {
  if (Target != nullptr) Target->remove_use(this);
  if (Target_ != nullptr) Target_->add_use(this);
  Target = Target_;
}

std::size_t Call::getNumArguments() const { return Arguments.size(); }

Instruction *Call::getArgument(std::size_t Index) const {
  assert(Index < getNumArguments());
  return Arguments[Index];
}

void Call::setArguments(std::span<Instruction *const> Arguments_) {
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
  for (auto *Argument : Arguments_)
    if (Argument != nullptr) Argument->add_use(this);
  Arguments = ranges::to<decltype(Arguments)>(Arguments_);
}

void Call::setArgument(std::size_t Index, Instruction *Argument) {
  assert(Index < getNumArguments());
  if (getArgument(Index) != nullptr) getArgument(Index)->remove_use(this);
  if (Argument != nullptr) Argument->add_use(this);
  Arguments[Index] = Argument;
}

void Call::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getTarget() == Old) setTarget(dyn_cast<Function>(New));
  for (std::size_t I = 0; I < getNumArguments(); ++I)
    if (getArgument(I) == Old) setArgument(I, dyn_cast<Instruction>(New));
}

bool Call::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Call;
}

bool Call::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Call::classof(dyn_cast<Instruction>(Node));
  return false;
}

/////////////////////////////// CallIndirect ///////////////////////////////////
CallIndirect::CallIndirect(
    Table *IndirectTable_, Instruction *Operand_,
    bytecode::FunctionType ExpectType_,
    std::span<Instruction *const> Arguments_)
    : Instruction(IKind::CallIndirect), IndirectTable(), Operand(),
      ExpectType(std::move(ExpectType_)), Arguments() {
  setIndirectTable(IndirectTable_);
  setOperand(Operand_);
  setArguments(Arguments_);
}

CallIndirect::~CallIndirect() noexcept {
  if (IndirectTable != nullptr) IndirectTable->remove_use(this);
  if (Operand != nullptr) Operand->remove_use(this);
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
}

Table *CallIndirect::getIndirectTable() const { return IndirectTable; }
Instruction *CallIndirect::getOperand() const { return Operand; }
bytecode::FunctionType const &CallIndirect::getExpectType() const {
  return ExpectType;
}

void CallIndirect::setIndirectTable(Table *IndirectTable_) {
  if (IndirectTable != nullptr) IndirectTable->remove_use(this);
  if (IndirectTable_ != nullptr) IndirectTable_->add_use(this);
  IndirectTable = IndirectTable_;
}

void CallIndirect::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void CallIndirect::setExpectType(bytecode::FunctionType Type_) {
  ExpectType = std::move(Type_);
}

std::span<Instruction *const> CallIndirect::getArguments() const {
  return Arguments;
}

std::size_t CallIndirect::getNumArguments() const { return Arguments.size(); }

Instruction *CallIndirect::getArgument(std::size_t Index) const {
  assert(Index < getNumArguments());
  return Arguments[Index];
}

void CallIndirect::setArguments(std::span<Instruction *const> Arguments_) {
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
  for (auto *Argument : Arguments_)
    if (Argument != nullptr) Argument->add_use(this);
  Arguments = ranges::to<decltype(Arguments)>(Arguments_);
}

void CallIndirect::setArgument(std::size_t Index, Instruction *Argument) {
  assert(Index < getNumArguments());
  if (getArgument(Index) != nullptr) getArgument(Index)->remove_use(this);
  if (Argument != nullptr) Argument->add_use(this);
  Arguments[Index] = Argument;
}

void CallIndirect::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getIndirectTable() == Old) setIndirectTable(dyn_cast<Table>(New));
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
  for (std::size_t I = 0; I < getNumArguments(); ++I)
    if (getArgument(I) == Old) setArgument(I, dyn_cast<Instruction>(New));
}

bool CallIndirect::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::CallIndirect;
}

bool CallIndirect::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return CallIndirect::classof(dyn_cast<Instruction>(Node));
  return false;
}

////////////////////////////////// Select //////////////////////////////////////
Select::Select(Instruction *Condition_, Instruction *True_, Instruction *False_)
    : Instruction(IKind::Select), Condition(), True(), False() {
  setCondition(Condition_);
  setTrue(True_);
  setFalse(False_);
}

Select::~Select() noexcept {
  if (Condition != nullptr) Condition->remove_use(this);
  if (True != nullptr) True->remove_use(this);
  if (False != nullptr) False->remove_use(this);
}

Instruction *Select::getCondition() const { return Condition; }
Instruction *Select::getTrue() const { return True; }
Instruction *Select::getFalse() const { return False; }

void Select::setCondition(Instruction *Condition_) {
  if (Condition != nullptr) Condition->remove_use(this);
  if (Condition_ != nullptr) Condition_->add_use(this);
  Condition = Condition_;
}

void Select::setTrue(Instruction *True_) {
  if (True != nullptr) True->remove_use(this);
  if (True_ != nullptr) True_->add_use(this);
  True = True_;
}

void Select::setFalse(Instruction *False_) {
  if (False != nullptr) False->remove_use(this);
  if (False_ != nullptr) False_->add_use(this);
  False = False_;
}

void Select::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getCondition() == Old) setCondition(dyn_cast<Instruction>(New));
  if (getTrue() == Old) setTrue(dyn_cast<Instruction>(New));
  if (getFalse() == Old) setFalse(dyn_cast<Instruction>(New));
}

bool Select::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Select;
}

bool Select::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Select::classof(dyn_cast<Select>(Node));
  return false;
}

//////////////////////////////// LocalGet //////////////////////////////////////
LocalGet::LocalGet(Local *Target_) : Instruction(IKind::LocalGet), Target() {
  setTarget(Target_);
}

LocalGet::~LocalGet() noexcept {
  if (Target != nullptr) Target->remove_use(this);
}

Local *LocalGet::getTarget() const { return Target; }

void LocalGet::setTarget(Local *Target_) {
  if (Target != nullptr) Target->remove_use(this);
  if (Target_ != nullptr) Target_->add_use(this);
  Target = Target_;
}

void LocalGet::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getTarget() == Old) setTarget(dyn_cast<Local>(New));
}

bool LocalGet::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::LocalGet;
}

bool LocalGet::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return LocalGet::classof(dyn_cast<Instruction>(Node));
  return false;
}

//////////////////////////////// LocalSet //////////////////////////////////////
LocalSet::LocalSet(Local *Target_, Instruction *Operand_)
    : Instruction(IKind::LocalSet), Target(), Operand() {
  setTarget(Target_);
  setOperand(Operand_);
}

LocalSet::~LocalSet() noexcept {
  if (Target != nullptr) Target->remove_use(this);
  if (Operand != nullptr) Operand->remove_use(this);
}

Local *LocalSet::getTarget() const { return Target; }
Instruction *LocalSet::getOperand() const { return Operand; }

void LocalSet::setTarget(Local *Target_) {
  if (Target != nullptr) Target->remove_use(this);
  if (Target_ != nullptr) Target_->add_use(this);
  Target = Target_;
}

void LocalSet::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void LocalSet::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getTarget() == Old) setTarget(dyn_cast<Local>(New));
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool LocalSet::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::LocalSet;
}

bool LocalSet::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return LocalSet::classof(dyn_cast<Instruction>(Node));
  return false;
}

//////////////////////////////// GlobalGet /////////////////////////////////////
GlobalGet::GlobalGet(Global *Target_)
    : Instruction(IKind::GlobalGet), Target() {
  setTarget(Target_);
}

GlobalGet::~GlobalGet() noexcept {
  if (Target != nullptr) Target->remove_use(this);
}

Global *GlobalGet::getTarget() const { return Target; }

void GlobalGet::setTarget(Global *Target_) {
  if (Target != nullptr) Target->remove_use(this);
  if (Target_ != nullptr) Target_->add_use(this);
  Target = Target_;
}

void GlobalGet::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getTarget() == Old) setTarget(dyn_cast<Global>(New));
}

bool GlobalGet::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::GlobalGet;
}

bool GlobalGet::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return GlobalGet::classof(dyn_cast<Instruction>(Node));
  return false;
}

//////////////////////////////// GlobalSet /////////////////////////////////////
GlobalSet::GlobalSet(Global *Target_, Instruction *Operand_)
    : Instruction(IKind::GlobalSet), Target(), Operand() {
  setTarget(Target_);
  setOperand(Operand_);
}

GlobalSet::~GlobalSet() noexcept {
  if (Target != nullptr) Target->remove_use(this);
  if (Operand != nullptr) Operand->remove_use(this);
}

Global *GlobalSet::getTarget() const { return Target; }
Instruction *GlobalSet::getOperand() const { return Operand; }

void GlobalSet::setTarget(Global *Target_) {
  if (Target != nullptr) Target->remove_use(this);
  if (Target_ != nullptr) Target_->add_use(this);
  Target = Target_;
}

void GlobalSet::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void GlobalSet::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getTarget() == Old) setTarget(dyn_cast<Global>(New));
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool GlobalSet::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::GlobalSet;
}

bool GlobalSet::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return GlobalSet::classof(dyn_cast<Instruction>(Node));
  return false;
}

///////////////////////////////// Constant /////////////////////////////////////
Constant::Constant(std::int32_t Value_)
    : Instruction(IKind::Constant), Value(Value_) {}
Constant::Constant(std::int64_t Value_)
    : Instruction(IKind::Constant), Value(Value_) {}
Constant::Constant(float Value_)
    : Instruction(IKind::Constant), Value(Value_) {}
Constant::Constant(double Value_)
    : Instruction(IKind::Constant), Value(Value_) {}
Constant::Constant(bytecode::V128Value Value_)
    : Instruction(IKind::Constant), Value(Value_) {}

// clang-format off
std::int32_t &Constant::asI32() { return std::get<std::int32_t>(Value); }
std::int64_t &Constant::asI64() { return std::get<std::int64_t>(Value); }
float &Constant::asF32() { return std::get<float>(Value); }
bytecode::V128Value &Constant::asV128()
{ return std::get<bytecode::V128Value>(Value); }

double &Constant::asF64() { return std::get<double>(Value); }
std::int32_t const &Constant::asI32() const
{ return std::get<std::int32_t>(Value); }
std::int64_t const &Constant::asI64() const
{ return std::get<std::int64_t>(Value); }
float const &Constant::asF32() const { return std::get<float>(Value); }
double const &Constant::asF64() const { return std::get<double>(Value); }
bytecode::V128Value const &Constant::asV128() const
{ return std::get<bytecode::V128Value>(Value); }
// clang-format on

bytecode::ValueType Constant::getValueType() const {
  auto const Visitor = utility::Overload{
      [](std::int32_t const &) { return bytecode::valuetypes::I32; },
      [](std::int64_t const &) { return bytecode::valuetypes::I64; },
      [](float const &) { return bytecode::valuetypes::F32; },
      [](double const &) { return bytecode::valuetypes::F64; },
      [](bytecode::V128Value const &) { return bytecode::valuetypes::V128; }};
  return std::visit(Visitor, Value);
}

void Constant::replace(ASTNode const *, ASTNode *) noexcept {
  utility::unreachable();
}

bool Constant::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Constant;
}

bool Constant::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Constant::classof(dyn_cast<Instruction>(Node));
  return false;
}

/////////////////////////////// IntUnaryOp /////////////////////////////////////
IntUnaryOp::IntUnaryOp(IntUnaryOperator Operator_, Instruction *Operand_)
    : Instruction(IKind::IntUnaryOp), Operator(Operator_), Operand() {
  setOperand(Operand_);
}

IntUnaryOp::~IntUnaryOp() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

IntUnaryOperator IntUnaryOp::getOperator() const { return Operator; }
Instruction *IntUnaryOp::getOperand() const { return Operand; }

void IntUnaryOp::setOperator(IntUnaryOperator Operator_) {
  Operator = Operator_;
}

void IntUnaryOp::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void IntUnaryOp::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool IntUnaryOp::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::IntUnaryOp;
}

bool IntUnaryOp::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return IntUnaryOp::classof(dyn_cast<Instruction>(Node));
  return false;
}

/////////////////////////////// IntBinaryOp ////////////////////////////////////
IntBinaryOp::IntBinaryOp(
    IntBinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_)
    : Instruction(IKind::IntBinaryOp), Operator(Operator_), LHS(), RHS() {
  setLHS(LHS_);
  setRHS(RHS_);
}

IntBinaryOp::~IntBinaryOp() noexcept {
  if (LHS != nullptr) LHS->remove_use(this);
  if (RHS != nullptr) RHS->remove_use(this);
}

IntBinaryOperator IntBinaryOp::getOperator() const { return Operator; }
Instruction *IntBinaryOp::getLHS() const { return LHS; }
Instruction *IntBinaryOp::getRHS() const { return RHS; }

void IntBinaryOp::setOperator(IntBinaryOperator Operator_) {
  Operator = Operator_;
}

void IntBinaryOp::setLHS(Instruction *LHS_) {
  if (LHS != nullptr) LHS->remove_use(this);
  if (LHS_ != nullptr) LHS_->add_use(this);
  LHS = LHS_;
}

void IntBinaryOp::setRHS(Instruction *RHS_) {
  if (RHS != nullptr) RHS->remove_use(this);
  if (RHS_ != nullptr) RHS_->add_use(this);
  RHS = RHS_;
}

void IntBinaryOp::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getLHS() == Old) setLHS(dyn_cast<Instruction>(New));
  if (getRHS() == Old) setRHS(dyn_cast<Instruction>(New));
}

bool IntBinaryOp::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::IntBinaryOp;
}

bool IntBinaryOp::classof(const ASTNode *Node) {
  if (Instruction::classof(Node))
    return IntBinaryOp::classof(dyn_cast<Instruction>(Node));
  return false;
}

//////////////////////////////// FPUnaryOp /////////////////////////////////////
FPUnaryOp::FPUnaryOp(FPUnaryOperator Operator_, Instruction *Operand_)
    : Instruction(IKind::FPUnaryOp), Operator(Operator_), Operand() {
  setOperand(Operand_);
}

FPUnaryOp::~FPUnaryOp() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

FPUnaryOperator FPUnaryOp::getOperator() const { return Operator; }
Instruction *FPUnaryOp::getOperand() const { return Operand; }
void FPUnaryOp::setOperator(FPUnaryOperator Operator_) { Operator = Operator_; }

void FPUnaryOp::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void FPUnaryOp::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool FPUnaryOp::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::FPUnaryOp;
}

bool FPUnaryOp::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return FPUnaryOp::classof(dyn_cast<Instruction>(Node));
  return false;
}

/////////////////////////////// FPBinaryOp /////////////////////////////////////
FPBinaryOp::FPBinaryOp(
    FPBinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_)
    : Instruction(IKind::FPBinaryOp), Operator(Operator_), LHS(), RHS() {
  setLHS(LHS_);
  setRHS(RHS_);
}

FPBinaryOp::~FPBinaryOp() noexcept {
  if (LHS != nullptr) LHS->remove_use(this);
  if (RHS != nullptr) RHS->remove_use(this);
}

FPBinaryOperator FPBinaryOp::getOperator() const { return Operator; }
Instruction *FPBinaryOp::getLHS() const { return LHS; }
Instruction *FPBinaryOp::getRHS() const { return RHS; }

void FPBinaryOp::setOperator(FPBinaryOperator Operator_) {
  Operator = Operator_;
}

void FPBinaryOp::setLHS(Instruction *LHS_) {
  if (LHS != nullptr) LHS->remove_use(this);
  if (LHS_ != nullptr) LHS_->add_use(this);
  LHS = LHS_;
}

void FPBinaryOp::setRHS(Instruction *RHS_) {
  if (RHS != nullptr) RHS->remove_use(this);
  if (RHS_ != nullptr) RHS_->add_use(this);
  RHS = RHS_;
}

void FPBinaryOp::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getLHS() == Old) setLHS(dyn_cast<Instruction>(New));
  if (getRHS() == Old) setRHS(dyn_cast<Instruction>(New));
}

bool FPBinaryOp::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::FPBinaryOp;
}

bool FPBinaryOp::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return FPBinaryOp::classof(dyn_cast<Instruction>(Node));
  return false;
}

////////////////////////////////// Load ////////////////////////////////////////
Load::Load(
    Memory *LinearMemory_, bytecode::ValueType Type_, Instruction *Address_,
    unsigned int LoadWidth_)
    : Instruction(IKind::Load), LinearMemory(), Type(Type_), Address(),
      LoadWidth(LoadWidth_) {
  setLinearMemory(LinearMemory_);
  setAddress(Address_);
}

Load::~Load() noexcept {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
  if (Address != nullptr) Address->remove_use(this);
}

bytecode::ValueType const &Load::getType() const { return Type; }
Memory *Load::getLinearMemory() const { return LinearMemory; }
Instruction *Load::getAddress() const { return Address; }
unsigned Load::getLoadWidth() const { return LoadWidth; }
void Load::setType(bytecode::ValueType const &Type_) { Type = Type_; }
void Load::setLoadWidth(unsigned LoadWidth_) { LoadWidth = LoadWidth_; }

void Load::setLinearMemory(Memory *LinearMemory_) {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
  if (LinearMemory_ != nullptr) LinearMemory_->add_use(this);
  LinearMemory = LinearMemory_;
}

void Load::setAddress(Instruction *Address_) {
  if (Address != nullptr) Address->remove_use(this);
  if (Address_ != nullptr) Address_->add_use(this);
  Address = Address_;
}

void Load::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getLinearMemory() == Old) setLinearMemory(dyn_cast<Memory>(New));
  if (getAddress() == Old) setAddress(dyn_cast<Instruction>(New));
}

bool Load::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Load;
}

bool Load::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Load::classof(dyn_cast<Instruction>(Node));
  return false;
}

///////////////////////////////// Store ////////////////////////////////////////
Store::Store(
    Memory *LinearMemory_, Instruction *Address_, Instruction *Operand_,
    unsigned int StoreWidth_)
    : Instruction(IKind::Store), LinearMemory(), Address(), Operand(),
      StoreWidth(StoreWidth_) {
  setLinearMemory(LinearMemory_);
  setAddress(Address_);
  setOperand(Operand_);
}

Store::~Store() noexcept {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
  if (Address != nullptr) Address->remove_use(this);
  if (Operand != nullptr) Operand->remove_use(this);
}

Memory *Store::getLinearMemory() const { return LinearMemory; }
Instruction *Store::getAddress() const { return Address; }
Instruction *Store::getOperand() const { return Operand; }
unsigned int Store::getStoreWidth() const { return StoreWidth; }
void Store::setStoreWidth(unsigned StoreWidth_) { StoreWidth = StoreWidth_; }

void Store::setLinearMemory(Memory *LinearMemory_) {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
  if (LinearMemory_ != nullptr) LinearMemory_->add_use(this);
  LinearMemory = LinearMemory_;
}

void Store::setAddress(Instruction *Address_) {
  if (Address != nullptr) Address->remove_use(this);
  if (Address_ != nullptr) Address_->add_use(this);
  Address = Address_;
}

void Store::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void Store::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getLinearMemory() == Old) setLinearMemory(dyn_cast<Memory>(New));
  if (getAddress() == Old) setAddress(dyn_cast<Instruction>(New));
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool Store::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Store;
}

bool Store::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Store::classof(dyn_cast<Instruction>(Node));
  return false;
}

////////////////////////////// MemoryGuard /////////////////////////////////////
MemoryGuard::MemoryGuard(
    Memory *LinearMemory_, Instruction *Address_, std::uint32_t GuardSize_)
    : Instruction(IKind::MemoryGuard), LinearMemory(), Address(),
      GuardSize(GuardSize_) {
  setLinearMemory(LinearMemory_);
  setAddress(Address_);
}

MemoryGuard::~MemoryGuard() noexcept {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
  if (Address != nullptr) Address->remove_use(this);
}

Memory *MemoryGuard::getLinearMemory() const { return LinearMemory; }
Instruction *MemoryGuard::getAddress() const { return Address; }
std::uint32_t MemoryGuard::getGuardSize() const { return GuardSize; }

void MemoryGuard::setLinearMemory(Memory *LinearMemory_) {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
  if (LinearMemory_ != nullptr) LinearMemory_->add_use(this);
  LinearMemory = LinearMemory_;
}

void MemoryGuard::setAddress(Instruction *Address_) {
  if (Address != nullptr) Address->remove_use(this);
  if (Address_ != nullptr) Address_->add_use(this);
  Address = Address_;
}

void MemoryGuard::setGuardSize(std::uint32_t GuardSize_) {
  GuardSize = GuardSize_;
}

void MemoryGuard::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getLinearMemory() == Old) setLinearMemory(dyn_cast<Memory>(New));
  if (getAddress() == Old) setAddress(dyn_cast<Instruction>(New));
}

bool MemoryGuard::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::MemoryGuard;
}

bool MemoryGuard::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return MemoryGuard::classof(dyn_cast<Instruction>(Node));
  return false;
}

/////////////////////////////// MemoryGrow /////////////////////////////////////
MemoryGrow::MemoryGrow(Memory *LinearMemory_, Instruction *Size_)
    : Instruction(InstructionKind::MemoryGrow), LinearMemory(), Size() {
  setLinearMemory(LinearMemory_);
  setSize(Size_);
}

MemoryGrow::~MemoryGrow() noexcept {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
  if (Size != nullptr) Size->remove_use(this);
}

Memory *MemoryGrow::getLinearMemory() const { return LinearMemory; }
Instruction *MemoryGrow::getSize() const { return Size; }

void MemoryGrow::setLinearMemory(Memory *LinearMemory_) {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
  if (LinearMemory_ != nullptr) LinearMemory_->add_use(this);
  LinearMemory = LinearMemory_;
}

void MemoryGrow::setSize(Instruction *Size_) {
  if (Size != nullptr) Size->remove_use(this);
  if (Size_ != nullptr) Size_->add_use(this);
  Size = Size_;
}

void MemoryGrow::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getLinearMemory() == Old) setLinearMemory(dyn_cast<Memory>(New));
  if (getSize() == Old) setSize(dyn_cast<Instruction>(New));
}

bool MemoryGrow::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::MemoryGrow;
}

bool MemoryGrow::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return MemoryGrow::classof(dyn_cast<Instruction>(Node));
  return false;
}

/////////////////////////////// MemorySize /////////////////////////////////////
MemorySize::MemorySize(Memory *LinearMemory_)
    : Instruction(InstructionKind::MemorySize), LinearMemory() {
  setLinearMemory(LinearMemory_);
}

MemorySize::~MemorySize() noexcept {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
}

Memory *MemorySize::getLinearMemory() const { return LinearMemory; }

void MemorySize::setLinearMemory(Memory *LinearMemory_) {
  if (LinearMemory != nullptr) LinearMemory->remove_use(this);
  if (LinearMemory_ != nullptr) LinearMemory_->add_use(this);
  LinearMemory = LinearMemory_;
}

void MemorySize::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getLinearMemory() == Old) setLinearMemory(dyn_cast<Memory>(New));
}

bool MemorySize::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::MemorySize;
}

bool MemorySize::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return MemorySize::classof(dyn_cast<Instruction>(Node));
  return false;
}

////////////////////////////////// Cast ////////////////////////////////////////
Cast::Cast(CastMode Mode_, bytecode::ValueType Type_, Instruction *Operand_)
    : Instruction(IKind::Cast), Mode(Mode_), Type(Type_), Operand() {
  setOperand(Operand_);
}

Cast::~Cast() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

CastMode Cast::getMode() const { return Mode; }
bytecode::ValueType const &Cast::getType() const { return Type; }
Instruction *Cast::getOperand() const { return Operand; }
void Cast::setMode(CastMode Mode_) { Mode = Mode_; }
void Cast::setType(const bytecode::ValueType &Type_) { Type = Type_; }

void Cast::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void Cast::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool Cast::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Cast;
}

bool Cast::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Cast::classof(dyn_cast<Instruction>(Node));
  return false;
}

///////////////////////////////// Extend ///////////////////////////////////////
Extend::Extend(Instruction *Operand_, unsigned int FromWidth_)
    : Instruction(IKind::Extend), Operand(), FromWidth(FromWidth_) {
  setOperand(Operand_);
}

Extend::~Extend() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

Instruction *Extend::getOperand() const { return Operand; }
unsigned Extend::getFromWidth() const { return FromWidth; }
void Extend::setFromWidth(unsigned FromWidth_) { FromWidth = FromWidth_; }

void Extend::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void Extend::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool Extend::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Extend;
}

bool Extend::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Extend::classof(dyn_cast<Instruction>(Node));
  return false;
}

////////////////////////////////// Pack ////////////////////////////////////////
Pack::Pack(std::span<Instruction *const> Arguments_)
    : Instruction(IKind::Pack) {
  setArguments(Arguments_);
}

Pack::~Pack() noexcept {
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
}

std::span<Instruction *const> Pack::getArguments() const { return Arguments; }

std::size_t Pack::getNumArguments() const { return Arguments.size(); }

Instruction *Pack::getArgument(std::size_t Index) const {
  assert(Index < getNumArguments());
  return Arguments[Index];
}

void Pack::setArguments(std::span<Instruction *const> Arguments_) {
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
  for (auto *Argument : Arguments_)
    if (Argument != nullptr) Argument->add_use(this);
  Arguments = ranges::to<decltype(Arguments)>(Arguments_);
}

void Pack::setArgument(std::size_t Index, Instruction *Argument) {
  assert(Index < getNumArguments());
  if (getArgument(Index) != nullptr) getArgument(Index)->remove_use(this);
  if (Argument != nullptr) Argument->add_use(this);
  Arguments[Index] = Argument;
}

void Pack::replace(ASTNode const *Old, ASTNode *New) noexcept {
  for (std::size_t I = 0; I < getNumArguments(); ++I)
    if (getArgument(I) == Old) setArgument(I, dyn_cast<Instruction>(New));
}

bool Pack::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Pack;
}

bool Pack::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Pack::classof(dyn_cast<Instruction>(Node));
  return false;
}

///////////////////////////////// Unpack ///////////////////////////////////////
Unpack::Unpack(Instruction *Operand_, unsigned int Index_)
    : Instruction(IKind::Unpack), Index(Index_), Operand() {
  setOperand(Operand_);
}

Unpack::~Unpack() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

unsigned Unpack::getIndex() const { return Index; }
Instruction *Unpack::getOperand() const { return Operand; }
void Unpack::setIndex(unsigned int Index_) { Index = Index_; }

void Unpack::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void Unpack::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool Unpack::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Unpack;
}

bool Unpack::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Unpack::classof(dyn_cast<Instruction>(Node));
  return false;
}

/////////////////////////////////// Phi ////////////////////////////////////////
Phi::Phi(bytecode::ValueType Type_) : Instruction(IKind::Phi), Type(Type_) {}

Phi::~Phi() noexcept {
  for (auto const &[Value, Path] : Candidates) {
    if (Value != nullptr) Value->remove_use(this);
    if (Path != nullptr) Path->remove_use(this);
  }
}

std::span<std::pair<Instruction *, BasicBlock *> const>
Phi::getCandidates() const {
  return Candidates;
}

std::size_t Phi::getNumCandidates() const { return Candidates.size(); }

std::pair<Instruction *, BasicBlock *>
Phi::getCandidate(std::size_t Index) const {
  assert(Index < getNumCandidates());
  return Candidates[Index];
}

void Phi::setCandidates(
    std::span<std::pair<Instruction *, BasicBlock *> const> Candidates_) {
  for (auto const &[Value, Path] : Candidates_) {
    if (Value != nullptr) Value->add_use(this);
    if (Path != nullptr) Path->add_use(this);
  }
  for (auto const &[Value, Path] : Candidates) {
    if (Value != nullptr) Value->remove_use(this);
    if (Path != nullptr) Path->remove_use(this);
  }
  Candidates = ranges::to<decltype(Candidates)>(Candidates_);
}

void Phi::setCandidate(
    std::size_t Index, Instruction *Value, BasicBlock *Flow) {
  assert(Index < getNumCandidates());
  auto [OldValue, OldFlow] = getCandidate(Index);
  if (OldValue != nullptr) OldValue->remove_use(this);
  if (OldFlow != nullptr) OldFlow->remove_use(this);
  if (Value != nullptr) Value->add_use(this);
  if (Flow != nullptr) Flow->add_use(this);
  Candidates[Index] = std::make_pair(Value, Flow);
}

bytecode::ValueType Phi::getType() const { return Type; }
void Phi::setType(bytecode::ValueType Type_) { Type = Type_; }

void Phi::addCandidate(Instruction *Value_, BasicBlock *Path_) {
  if (Value_ != nullptr) Value_->add_use(this);
  if (Path_ != nullptr) Path_->add_use(this);
  Candidates.emplace_back(Value_, Path_);
}

void Phi::replace(ASTNode const *Old, ASTNode *New) noexcept {
  for (auto &[Value, Flow] : Candidates) {
    if (Value == Old) {
      if (Value != nullptr) Value->remove_use(this);
      if (New != nullptr) New->add_use(this);
      Value = dyn_cast<Instruction>(New);
    }
    if (Flow == Old) {
      if (Flow != nullptr) Flow->remove_use(this);
      if (New != nullptr) New->add_use(this);
      Flow = dyn_cast<BasicBlock>(New);
    }
  }
}

bool Phi::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Phi;
}

bool Phi::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Phi::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions