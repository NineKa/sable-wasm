#include "Instruction.h"
#include "Module.h"

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/replace.hpp>

#define IF_THEN_SET(Operand, Value, ToValue)                                   \
  if (Operand == Value) {                                                      \
    Value = ToValue;                                                           \
    return;                                                                    \
  }

namespace mir {
BasicBlock::BasicBlock(Function *Parent_)
    : ASTNode(ASTNodeKind::BasicBlock), Parent(Parent_) {}

llvm::ilist<Instruction> BasicBlock::*
BasicBlock::getSublistAccess(Instruction *) {
  return &BasicBlock::Instructions;
}
} // namespace mir

namespace mir::instructions {
using IKind = InstructionKind;
///////////////////////////////// Unreachable //////////////////////////////////
Unreachable::Unreachable(BasicBlock *Parent_)
    : Instruction(IKind::Unreachable, Parent_) {}

void Unreachable::detach(ASTNode const *) noexcept { utility::unreachable(); }

bool Unreachable::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Unreachable;
}

bool Unreachable::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Unreachable::classof(dyn_cast<Instruction>(Node));
  return false;
}

//////////////////////////////////// Branch ////////////////////////////////////
Branch::Branch(
    BasicBlock *Parent_, Instruction *Condition_, BasicBlock *Target_,
    BasicBlock *FalseTarget_)
    : Instruction(IKind::Branch, Parent_), Condition(), Target(),
      FalseTarget() {
  setCondition(Condition_);
  setTarget(Target_);
  setFalseTarget(FalseTarget_);
}

Branch::Branch(BasicBlock *Parent_, BasicBlock *Target_)
    : Instruction(IKind::Branch, Parent_), Condition(), Target(),
      FalseTarget() {
  setTarget(Target_);
}

Branch::~Branch() noexcept {
  if (Condition != nullptr) Condition->remove_use(this);
  if (Target != nullptr) Target->remove_use(this);
  if (FalseTarget != nullptr) FalseTarget->remove_use(this);
}

bool Branch::isConditional() const { return Condition != nullptr; }
bool Branch::isUnconditional() const { return Condition == nullptr; }
Instruction *Branch::getCondition() const { return Condition; }
BasicBlock *Branch::getTarget() const { return Target; }
BasicBlock *Branch::getFalseTarget() const { return FalseTarget; }

void Branch::setCondition(Instruction *Condition_) {
  if (Condition != nullptr) Condition->remove_use(this);
  if (Condition_ != nullptr) Condition_->add_use(this);
  Condition = Condition_;
}

void Branch::setTarget(BasicBlock *Target_) {
  if (Target != nullptr) Target->remove_use(this);
  if (Target_ != nullptr) Target_->add_use(this);
  Target = Target_;
}

void Branch::setFalseTarget(BasicBlock *FalseTarget_) {
  if (FalseTarget != nullptr) FalseTarget->remove_use(this);
  if (FalseTarget_ != nullptr) FalseTarget_->add_use(this);
  FalseTarget = FalseTarget_;
}

void Branch::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Condition, nullptr)
  IF_THEN_SET(Node, Target, nullptr)
  IF_THEN_SET(Node, FalseTarget, nullptr)
  utility::unreachable();
}

bool Branch::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Branch;
}

bool Branch::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Branch::classof(dyn_cast<Instruction>(Node));
  return false;
}

///////////////////////////////// BranchTable //////////////////////////////////
BranchTable::BranchTable(
    BasicBlock *Parent_, Instruction *Operand_, BasicBlock *DefaultTarget_,
    std::span<BasicBlock *const> Targets_)
    : Instruction(IKind::BranchTable, Parent_), Operand(), DefaultTarget(),
      Targets() {
  setOperand(Operand_);
  setDefaultTarget(DefaultTarget_);
  setTargets(Targets_);
}

BranchTable::~BranchTable() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
  if (DefaultTarget != nullptr) DefaultTarget->remove_use(this);
  for (auto *Target : Targets)
    if (Target != nullptr) Target->remove_use(this);
}

Instruction *BranchTable::getOperand() const { return Operand; }
BasicBlock *BranchTable::getDefaultTarget() const { return DefaultTarget; }
std::span<BasicBlock *const> BranchTable::getTargets() const { return Targets; }

void BranchTable::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void BranchTable::setDefaultTarget(BasicBlock *DefaultTarget_) {
  if (DefaultTarget != nullptr) DefaultTarget->remove_use(this);
  if (DefaultTarget_ != nullptr) DefaultTarget_->add_use(this);
  DefaultTarget = DefaultTarget_;
}

void BranchTable::setTargets(std::span<BasicBlock *const> Targets_) {
  for (auto *Target : Targets)
    if (Target != nullptr) Target->remove_use(this);
  for (auto *Target : Targets_)
    if (Target != nullptr) Target->add_use(this);
  Targets = ranges::to<decltype(Targets)>(Targets_);
}

void BranchTable::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Operand, nullptr)
  IF_THEN_SET(Node, DefaultTarget, nullptr)
  if (ranges::contains(Targets, Node)) {
    ranges::replace(Targets, Node, nullptr);
    return;
  }
  utility::unreachable();
}

bool BranchTable::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::BranchTable;
}

bool BranchTable::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return BranchTable::classof(dyn_cast<Instruction>(Node));
  return false;
}

/////////////////////////////////// Return /////////////////////////////////////
Return::Return(BasicBlock *Parent_)
    : Instruction(IKind::Return, Parent_), Operand() {}

Return::Return(BasicBlock *Parent_, Instruction *Operand_)
    : Instruction(IKind::Return, Parent_), Operand() {
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

void Return::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Operand, nullptr)
  utility::unreachable();
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
Call::Call(
    BasicBlock *Parent_, Function *Target_,
    std::span<Instruction *const> Arguments_)
    : Instruction(IKind::Call, Parent_), Target(), Arguments() {
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

void Call::setArguments(std::span<Instruction *const> Arguments_) {
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
  for (auto *Argument : Arguments_)
    if (Argument != nullptr) Argument->add_use(this);
  Arguments = ranges::to<decltype(Arguments)>(Arguments_);
}

void Call::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Target, nullptr)
  if (ranges::contains(Arguments, Node)) {
    ranges::replace(Arguments, Node, nullptr);
    return;
  }
  utility::unreachable();
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
    BasicBlock *Parent_, Table *IndirectTable_, Instruction *Operand_,
    bytecode::FunctionType ExpectType_,
    std::span<Instruction *const> Arguments_)
    : Instruction(IKind::CallIndirect, Parent_), IndirectTable(), Operand(),
      ExpectType(std::move(ExpectType_)), Arguments() {
  setIndirectTable(IndirectTable_);
  setOperand(Operand_);
  setArguments(Arguments_);
}

CallIndirect::~CallIndirect() noexcept {
  if (IndirectTable != nullptr) IndirectTable->remove_use(this);
  if (Operand != nullptr) Operand->remove_use(this);
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

void CallIndirect::setArguments(std::span<Instruction *const> Arguments_) {
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
  for (auto *Argument : Arguments_)
    if (Argument != nullptr) Argument->add_use(this);
  Arguments = ranges::to<decltype(Arguments)>(Arguments_);
}

void CallIndirect::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, IndirectTable, nullptr)
  IF_THEN_SET(Node, Operand, nullptr)
  if (ranges::contains(Arguments, Node)) {
    ranges::replace(Arguments, Node, nullptr);
    return;
  }
  utility::unreachable();
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
Select::Select(
    BasicBlock *Parent_, Instruction *Condition_, Instruction *True_,
    Instruction *False_)
    : Instruction(IKind::Select, Parent_), Condition(), True(), False() {
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

void Select::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Condition, nullptr)
  IF_THEN_SET(Node, True, nullptr)
  IF_THEN_SET(Node, False, nullptr)
  utility::unreachable();
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
LocalGet::LocalGet(BasicBlock *Parent_, Local *Target_)
    : Instruction(IKind::LocalGet, Parent_), Target() {
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

void LocalGet::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Target, nullptr)
  utility::unreachable();
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
LocalSet::LocalSet(BasicBlock *Parent_, Local *Target_, Instruction *Operand_)
    : Instruction(IKind::LocalSet, Parent_), Target(), Operand() {
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
  if (Operand != nullptr) Target->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void LocalSet::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Target, nullptr)
  IF_THEN_SET(Node, Operand, nullptr)
  utility::unreachable();
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
GlobalGet::GlobalGet(BasicBlock *Parent_, Global *Target_)
    : Instruction(IKind::GlobalGet, Parent_), Target() {
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

void GlobalGet::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Target, nullptr)
  utility::unreachable();
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
GlobalSet::GlobalSet(
    BasicBlock *Parent_, Global *Target_, Instruction *Operand_)
    : Instruction(IKind::GlobalSet, Parent_), Target(), Operand() {
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

void GlobalSet::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Target, nullptr)
  IF_THEN_SET(Node, Operand, nullptr)
  utility::unreachable();
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
Constant::Constant(BasicBlock *Parent_, std::int32_t Value_)
    : Instruction(IKind::Constant, Parent_), Value(Value_) {}
Constant::Constant(BasicBlock *Parent_, std::int64_t Value_)
    : Instruction(IKind::Constant, Parent_), Value(Value_) {}
Constant::Constant(BasicBlock *Parent_, float Value_)
    : Instruction(IKind::Constant, Parent_), Value(Value_) {}
Constant::Constant(BasicBlock *Parent_, double Value_)
    : Instruction(IKind::Constant, Parent_), Value(Value_) {}

std::int32_t &Constant::asI32() { return std::get<std::int32_t>(Value); }
std::int64_t &Constant::asI64() { return std::get<std::int64_t>(Value); }
float &Constant::asF32() { return std::get<float>(Value); }
double &Constant::asF64() { return std::get<double>(Value); }
std::int32_t Constant::asI32() const { return std::get<std::int32_t>(Value); }
std::int64_t Constant::asI64() const { return std::get<std::int64_t>(Value); }
float Constant::asF32() const { return std::get<float>(Value); }
double Constant::asF64() const { return std::get<double>(Value); }

bytecode::ValueType Constant::getValueType() const {
  auto const Visitor = utility::Overload{
      [](std::int32_t const &) { return bytecode::valuetypes::I32; },
      [](std::int64_t const &) { return bytecode::valuetypes::I64; },
      [](float const &) { return bytecode::valuetypes::F32; },
      [](double const &) { return bytecode::valuetypes::F64; }};
  return std::visit(Visitor, Value);
}

void Constant::detach(ASTNode const *) noexcept { utility::unreachable(); }

bool Constant::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == IKind::Constant;
}

bool Constant::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Constant::classof(dyn_cast<Instruction>(Node));
  return false;
}

/////////////////////////////// IntUnaryOp /////////////////////////////////////
IntUnaryOp::IntUnaryOp(
    BasicBlock *Parent_, IntUnaryOperator Operator_, Instruction *Operand_)
    : Instruction(IKind::IntUnaryOp, Parent_), Operator(Operator_), Operand() {
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

void IntUnaryOp::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Operand, nullptr)
  utility::unreachable();
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
    BasicBlock *Parent_, IntBinaryOperator Operator_, Instruction *LHS_,
    Instruction *RHS_)
    : Instruction(IKind::IntBinaryOp, Parent_), Operator(Operator_), LHS(),
      RHS() {
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

void IntBinaryOp::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, LHS, nullptr)
  IF_THEN_SET(Node, RHS, nullptr)
  utility::unreachable();
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
FPUnaryOp::FPUnaryOp(
    BasicBlock *Parent_, FPUnaryOperator Operator_, Instruction *Operand_)
    : Instruction(IKind::FPUnaryOp, Parent_), Operator(Operator_), Operand() {
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

void FPUnaryOp::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Operand, nullptr)
  utility::unreachable();
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
    BasicBlock *Parent_, FPBinaryOperator Operator_, Instruction *LHS_,
    Instruction *RHS_)
    : Instruction(IKind::FPBinaryOp, Parent_), Operator(Operator_), LHS(),
      RHS() {
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

void FPBinaryOp::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, LHS, nullptr)
  IF_THEN_SET(Node, RHS, nullptr)
  utility::unreachable();
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
    BasicBlock *Parent_, Memory *LinearMemory_, bytecode::ValueType Type_,
    Instruction *Address_, unsigned int LoadWidth_)
    : Instruction(IKind::Load, Parent_), LinearMemory(), Type(Type_), Address(),
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

void Load::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Address, nullptr)
  IF_THEN_SET(Node, LinearMemory, nullptr)
  utility::unreachable();
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
    BasicBlock *Parent_, Memory *LinearMemory_, Instruction *Address_,
    Instruction *Operand_, unsigned int StoreWidth_)
    : Instruction(IKind::Store, Parent_), LinearMemory(), Address(), Operand(),
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

void Store::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Address, nullptr)
  IF_THEN_SET(Node, Operand, nullptr)
  IF_THEN_SET(Node, LinearMemory, nullptr)
  utility::unreachable();
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
    BasicBlock *Parent_, Memory *LinearMemory_, Instruction *Address_,
    std::uint32_t GuardSize_)
    : Instruction(IKind::MemoryGuard, Parent_), LinearMemory(), Address(),
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

void MemoryGuard::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Address, nullptr)
  IF_THEN_SET(Node, LinearMemory, nullptr)
  utility::unreachable();
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
MemoryGrow::MemoryGrow(
    BasicBlock *Parent_, Memory *LinearMemory_, Instruction *Size_)
    : Instruction(InstructionKind::MemoryGrow, Parent_), LinearMemory(),
      Size() {
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

void MemoryGrow::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, LinearMemory, nullptr)
  IF_THEN_SET(Node, Size, nullptr)
  utility::unreachable();
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
MemorySize::MemorySize(BasicBlock *Parent_, Memory *LinearMemory_)
    : Instruction(InstructionKind::MemorySize, Parent_), LinearMemory() {
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

void MemorySize::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, LinearMemory, nullptr);
  utility::unreachable();
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
Cast::Cast(
    BasicBlock *Parent_, CastMode Mode_, bytecode::ValueType Type_,
    Instruction *Operand_)
    : Instruction(IKind::Cast, Parent_), Mode(Mode_), Type(Type_), Operand() {
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

void Cast::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Operand, nullptr)
  utility::unreachable();
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
Extend::Extend(
    BasicBlock *Parent_, Instruction *Operand_, unsigned int FromWidth_)
    : Instruction(IKind::Extend, Parent_), Operand(), FromWidth(FromWidth_) {
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

void Extend::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Operand, nullptr)
  utility::unreachable();
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
Pack::Pack(BasicBlock *Parent_, std::span<Instruction *const> Arguments_)
    : Instruction(IKind::Pack, Parent_) {
  setArguments(Arguments_);
}

Pack::~Pack() noexcept {
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
}

std::span<Instruction *const> Pack::getArguments() const { return Arguments; }

void Pack::setArguments(std::span<Instruction *const> Arguments_) {
  for (auto *Argument : Arguments)
    if (Argument != nullptr) Argument->remove_use(this);
  for (auto *Argument : Arguments_)
    if (Argument != nullptr) Argument->add_use(this);
  Arguments = ranges::to<decltype(Arguments)>(Arguments_);
}

void Pack::detach(ASTNode const *Node) noexcept {
  if (ranges::contains(Arguments, Node)) {
    ranges::replace(Arguments, Node, nullptr);
    return;
  }
  utility::unreachable();
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
Unpack::Unpack(BasicBlock *Parent_, Instruction *Operand_, unsigned int Index_)
    : Instruction(IKind::Unpack, Parent_), Index(Index_), Operand() {
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

void Unpack::detach(ASTNode const *Node) noexcept {
  IF_THEN_SET(Node, Operand, nullptr)
  utility::unreachable();
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
Phi::Phi(BasicBlock *Parent_, bytecode::ValueType Type_)
    : Instruction(IKind::Phi, Parent_), Type(Type_) {}

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

bytecode::ValueType Phi::getType() const { return Type; }
void Phi::setType(bytecode::ValueType Type_) { Type = Type_; }

void Phi::addCandidate(Instruction *Value_, BasicBlock *Path_) {
  if (Value_ != nullptr) Value_->add_use(this);
  if (Path_ != nullptr) Path_->add_use(this);
  Candidates.emplace_back(Value_, Path_);
}

void Phi::detach(ASTNode const *Node) noexcept {
  for (auto &&[Value, Path] : Candidates) {
    IF_THEN_SET(Node, Value, nullptr)
    IF_THEN_SET(Node, Path, nullptr)
  }
  utility::unreachable();
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

namespace fmt {
char const *formatter<mir::InstructionKind>::getEnumString(
    mir::InstructionKind const &Kind) {
  using IKind = mir::InstructionKind;
  // clang-format off
  switch (Kind) {
  case IKind::Unreachable : return "unreachable";
  case IKind::Branch      : return "br";
  case IKind::BranchTable : return "br.table";
  case IKind::Return      : return "ret";
  case IKind::Call        : return "call";
  case IKind::CallIndirect: return "call_indirect";
  case IKind::Select      : return "select";
  case IKind::LocalGet    : return "local.get";
  case IKind::LocalSet    : return "local.set";
  case IKind::GlobalGet   : return "global.get";
  case IKind::GlobalSet   : return "global.set";
  case IKind::Constant    : return "const";
  case IKind::IntUnaryOp  : return "int.unary";
  case IKind::IntBinaryOp : return "int.binary";
  case IKind::FPUnaryOp   : return "fp.unary";
  case IKind::FPBinaryOp  : return "fp.binary";
  case IKind::Load        : return "load";
  case IKind::Store       : return "store";
  case IKind::MemoryGuard : return "memory.guard";
  case IKind::MemoryGrow  : return "memory.grow";
  case IKind::MemorySize  : return "memory.size";
  case IKind::Cast        : return "cast";
  case IKind::Extend      : return "extend";
  case IKind::Pack        : return "pack";
  case IKind::Unpack      : return "unpack";
  case IKind::Phi         : return "phi";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt
