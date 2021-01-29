#include "Instruction.h"

#include "../utility/Commons.h"

namespace mir {
namespace {
namespace detail {
struct OperandBeginVisitor
    : InstVisitor<OperandBeginVisitor, Instruction::operand_iterator, false> {
  template <instruction T> Instruction::operand_iterator operator()(T *Inst) {
    return Inst->operand_begin();
  }
};
struct OperandEndVisitor
    : InstVisitor<OperandEndVisitor, Instruction::operand_iterator, false> {
  template <instruction T> Instruction::operand_iterator operator()(T *Inst) {
    return Inst->operand_end();
  }
};
struct BasicBlockBeginVisitor
    : InstVisitor<BasicBlockBeginVisitor, Instruction::bb_iterator, false> {
  using RetType = Instruction::bb_iterator;
  RetType operator()(mir::instructions::Branch *Inst) {
    return Inst->basic_block_begin();
  }
  template <instruction T> RetType operator()(T *) { return nullptr; }
};
struct BasicBlockEndVisitor
    : InstVisitor<BasicBlockEndVisitor, Instruction::bb_iterator, false> {
  using RetType = Instruction::bb_iterator;
  RetType operator()(mir::instructions::Branch *Inst) {
    return Inst->basic_block_end();
  }
  template <instruction T> RetType operator()(T *) { return nullptr; }
};
} // namespace detail
} // namespace

Instruction::operand_iterator Instruction::operand_begin() {
  return detail::OperandBeginVisitor{}.visit(this);
}

Instruction::operand_iterator Instruction::operand_end() {
  return detail::OperandEndVisitor{}.visit(this);
}

Instruction::bb_iterator Instruction::basic_block_begin() {
  return detail::BasicBlockBeginVisitor{}.visit(this);
}

Instruction::bb_iterator Instruction::basic_block_end() {
  return detail::BasicBlockEndVisitor{}.visit(this);
}

Instruction::~Instruction() noexcept {
  for (auto *UsedSite : UsedSites) {
    auto *OperandBegin = UsedSite->operand_begin();
    auto *OperandEnd = UsedSite->operand_end();
    for (auto *OperandIter = OperandBegin; OperandIter != OperandEnd;
         std::advance(OperandIter, 1)) {
      if (*OperandIter == this) *OperandIter = nullptr;
    }
  }
}
} // namespace mir

namespace mir::instructions {
using IKind = InstructionKind;
///////////////////////////////// Unreachable //////////////////////////////////
Unreachable::Unreachable(BasicBlock *Parent)
    : Instruction(IKind::Unreachable, Parent) {}

///////////////////////////////// Branch ////////////////////////////////////
Branch::Branch(BasicBlock *Parent, BasicBlock *Target_)
    : Instruction(IKind::Branch, Parent), Target({nullptr}),
      Operands({nullptr}) {
  setTarget(Target_);
}
Branch::Branch(BasicBlock *Parent, BasicBlock *Target_, Instruction *Condition_)
    : Instruction(IKind::Branch, Parent), Target({nullptr}),
      Operands({nullptr}) {
  setTarget(Target_);
  setCondition(Condition_);
}

////////////////////////////////// Constant ////////////////////////////////////
Constant::Constant(BasicBlock *Parent, std::int32_t Value)
    : Instruction(IKind::Constant, Parent), Storage(Value) {}
Constant::Constant(BasicBlock *Parent, std::int64_t Value)
    : Instruction(IKind::Constant, Parent), Storage(Value) {}
Constant::Constant(BasicBlock *Parent, float Value)
    : Instruction(IKind::Constant, Parent), Storage(Value) {}
Constant::Constant(BasicBlock *Parent, double Value)
    : Instruction(IKind::Constant, Parent), Storage(Value) {}

////////////////////////////////// IntUnaryOp //////////////////////////////////
IntUnaryOp::IntUnaryOp(
    BasicBlock *Parent, IntUnaryOperator Operator_, Instruction *Operand)
    : Instruction(IKind::IntUnaryOp, Parent), Operator(Operator_),
      Operands({nullptr}) {
  setOperand(Operand);
}

Instruction *IntUnaryOp::getOperand() const { return Operands[0]; }
IntUnaryOperator IntUnaryOp::getOperator() const { return Operator; }

void IntUnaryOp::setOperand(Instruction *Operand) {
  if (getOperand() != nullptr) getOperand()->removeUsedSite(this);
  if (Operand != nullptr) Operand->addUsedSite(this);
  Operands[0] = Operand;
}

///////////////////////////////// IntBinaryOp //////////////////////////////////
IntBinaryOp::IntBinaryOp(
    BasicBlock *Parent, IntBinaryOperator Operator_, Instruction *LHS_,
    Instruction *RHS_)
    : Instruction(IKind::IntBinaryOp, Parent), Operator(Operator_),
      Operands({nullptr, nullptr}) {
  setLHS(LHS_);
  setRHS(RHS_);
}

Instruction *IntBinaryOp::getLHS() const { return Operands[0]; }
Instruction *IntBinaryOp::getRHS() const { return Operands[1]; }
IntBinaryOperator IntBinaryOp::getOperator() const { return Operator; }

void IntBinaryOp::setLHS(Instruction *Operand) {
  if (getLHS() != nullptr) getLHS()->removeUsedSite(this);
  if (Operand != nullptr) Operand->addUsedSite(this);
  Operands[0] = Operand;
}

void IntBinaryOp::setRHS(Instruction *Operand) {
  if (getRHS() != nullptr) getRHS()->removeUsedSite(this);
  if (Operand != nullptr) Operand->addUsedSite(this);
  Operands[1] = Operand;
}

bool IntBinaryOp::isComparison() const {
  using Op = IntBinaryOperator;
  return (Operator == Op::Eq) || (Operator == Op::Ne) ||
         (Operator == Op::LtS) || (Operator == Op::LtU) ||
         (Operator == Op::GtS) || (Operator == Op::GtU) ||
         (Operator == Op::LeS) || (Operator == Op::LeU) ||
         (Operator == Op::GeS) || (Operator == Op::GeU);
}

bool IntBinaryOp::isMulOrDiv() const {
  using Op = IntBinaryOperator;
  return (Operator == Op::DivS) || (Operator == Op::DivU) ||
         (Operator == Op::RemS) || (Operator == Op::RemU);
}

////////////////////////////////// FPUnaryOp ///////////////////////////////////
FPUnaryOp::FPUnaryOp(
    BasicBlock *Parent, FPUnaryOperator Operator_, Instruction *Operand_)
    : Instruction(IKind::FPUnaryOp, Parent), Operator(Operator_),
      Operands({nullptr}) {
  setOperand(Operand_);
}

Instruction *FPUnaryOp::getOperand() const { return Operands[0]; }
FPUnaryOperator FPUnaryOp::getOperator() const { return Operator; }

void FPUnaryOp::setOperand(Instruction *Operand) {
  if (getOperand() != nullptr) getOperand()->removeUsedSite(this);
  if (Operand != nullptr) Operand->addUsedSite(this);
  Operands[0] = Operand;
}

///////////////////////////////// FPBinaryOp ///////////////////////////////////
FPBinaryOp::FPBinaryOp(
    BasicBlock *Parent, FPBinaryOperator Operator_, Instruction *LHS_,
    Instruction *RHS_)
    : Instruction(IKind::FPBinaryOp, Parent), Operator(Operator_),
      Operands({nullptr, nullptr}) {
  setLHS(LHS_);
  setRHS(RHS_);
}

Instruction *FPBinaryOp::getLHS() const { return Operands[0]; }
Instruction *FPBinaryOp::getRHS() const { return Operands[1]; }
FPBinaryOperator FPBinaryOp::getOperator() const { return Operator; }

void FPBinaryOp::setLHS(Instruction *Operand) {
  if (getLHS() != nullptr) getLHS()->removeUsedSite(this);
  if (Operand != nullptr) Operand->addUsedSite(this);
  Operands[0] = Operand;
}

void FPBinaryOp::setRHS(Instruction *Operand) {
  if (getRHS() != nullptr) getRHS()->removeUsedSite(this);
  if (Operand != nullptr) Operand->addUsedSite(this);
  Operands[1] = Operand;
}

bool FPBinaryOp::isComparison() const {
  using Op = FPBinaryOperator;
  return (Operator == Op::Eq) || (Operator == Op::Ne) || (Operator == Op::Lt) ||
         (Operator == Op::Gt) || (Operator == Op::Le) || (Operator == Op::Ge);
}

} // namespace mir::instructions
