#include "Branch.h"
#include "BasicBlock.h"

namespace mir::instructions {
Branch::Branch(BranchKind Kind_)
    : Instruction(InstructionKind::Branch), Kind(Kind_) {}
Branch::~Branch() noexcept = default;

BranchKind Branch::getBranchKind() const { return Kind; }

bool Branch::isConditional() const { return Kind == BranchKind::Conditional; }
bool Branch::isSwitch() const { return Kind == BranchKind::Switch; }
bool Branch::isUnconditional() const {
  return Kind == BranchKind::Unconditional;
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
} // namespace mir::instructions