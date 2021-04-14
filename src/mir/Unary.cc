#include "Unary.h"

namespace mir::instructions {
Unary::Unary(UnaryKind Kind_, Instruction *Operand_)
    : Instruction(InstructionKind::Unary), Kind(Kind_), Operand() {
  setOperand(Operand_);
}

Unary::~Unary() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

UnaryKind Unary::getUnaryKind() const { return Kind; }
bool Unary::isIntUnary() const { return Kind == UnaryKind::IntUnary; }
bool Unary::isFPUnary() const { return Kind == UnaryKind::FPUnary; }
bool Unary::isSIMD128Unary() const { return Kind == UnaryKind::SIMD128Unary; }

bool Unary::isSIMD128IntUnary() const {
  return Kind == UnaryKind::SIMD128IntUnary;
}

bool Unary::isSIMD128FPUnary() const {
  return Kind == UnaryKind::SIMD128FPUnary;
}

Instruction *Unary::getOperand() const { return Operand; }

void Unary::setOperand(Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void Unary::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool Unary::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == InstructionKind::Unary;
}

bool Unary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Unary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions

namespace mir::instructions::unary {
IntUnary::IntUnary(IntUnaryOperator Operator_, Instruction *Operand_)
    : Unary(UnaryKind::IntUnary, Operand_), Operator(Operator_) {}
IntUnary::~IntUnary() noexcept = default;
IntUnaryOperator IntUnary::getOperator() const { return Operator; }
void IntUnary::setOperator(IntUnaryOperator Operator_) { Operator = Operator_; }

bool IntUnary::classof(Unary const *Inst) { return Inst->isIntUnary(); }

bool IntUnary::classof(Instruction const *Inst) {
  if (Unary::classof(Inst)) return IntUnary::classof(dyn_cast<Unary>(Inst));
  return false;
}

bool IntUnary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return IntUnary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::unary

namespace fmt {
char const *formatter<mir::instructions::unary::IntUnaryOperator>::toString(
    UnaryOperator const &Operator) {
  switch (Operator) {
  case UnaryOperator::Eqz: return "int.eqz";
  case UnaryOperator::Clz: return "int.clz";
  case UnaryOperator::Ctz: return "int.ctz";
  case UnaryOperator::Popcnt: return "int.popcnt";
  default: utility::unreachable();
  }
}
} // namespace fmt

namespace mir::instructions::unary {
FPUnary::FPUnary(FPUnaryOperator Operator_, Instruction *Operand_)
    : Unary(UnaryKind::FPUnary, Operand_), Operator(Operator_) {}
FPUnary::~FPUnary() noexcept = default;
FPUnaryOperator FPUnary::getOperator() const { return Operator; }
void FPUnary::setOperator(FPUnaryOperator Operator_) { Operator = Operator_; }

bool FPUnary::classof(Unary const *Inst) { return Inst->isFPUnary(); }

bool FPUnary::classof(Instruction const *Inst) {
  if (Unary::classof(Inst)) return FPUnary::classof(dyn_cast<Unary>(Inst));
  return false;
}

bool FPUnary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return FPUnary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::unary

namespace fmt {
char const *formatter<mir::instructions::unary::FPUnaryOperator>::toString(
    UnaryOperator const &Operator) {
  // clang-format off
  switch (Operator) {
  case UnaryOperator::Abs    : return "fp.abs";
  case UnaryOperator::Neg    : return "fp.neg";
  case UnaryOperator::Ceil   : return "fp.ceil";
  case UnaryOperator::Floor  : return "fp.floor";
  case UnaryOperator::Trunc  : return "fp.trunc";
  case UnaryOperator::Nearest: return "fp.nearest";
  case UnaryOperator::Sqrt   : return "fp.sqrt";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt

namespace mir::instructions::unary {
SIMD128Unary::SIMD128Unary(
    SIMD128UnaryOperator Operator_, Instruction *Operand_)
    : Unary(UnaryKind::SIMD128Unary, Operand_), Operator(Operator_) {}
SIMD128Unary::~SIMD128Unary() noexcept = default;

SIMD128UnaryOperator SIMD128Unary::getOperator() const { return Operator; }

void SIMD128Unary::setOperator(SIMD128UnaryOperator Operator_) {
  Operator = Operator_;
}

bool SIMD128Unary::classof(Unary const *Inst) { return Inst->isSIMD128Unary(); }

bool SIMD128Unary::classof(Instruction const *Inst) {
  if (Unary::classof(Inst)) return SIMD128Unary::classof(dyn_cast<Unary>(Inst));
  return false;
}

bool SIMD128Unary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128Unary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::unary

namespace fmt {
char const *formatter<mir::instructions::unary::SIMD128UnaryOperator>::toString(
    UnaryOperator const &Operator) {
  switch (Operator) {
  case UnaryOperator::AnyTrue: return "v128.anytrue";
  case UnaryOperator::Not: return "v128.not";
  default: utility::unreachable();
  }
}
} // namespace fmt

namespace mir::instructions::unary {
SIMD128IntUnary::SIMD128IntUnary(
    SIMD128IntUnaryOperator Operator_, SIMD128IntLaneInfo LaneInfo_,
    Instruction *Operand_)
    : Unary(UnaryKind::SIMD128IntUnary, Operand_), Operator(Operator_),
      LaneInfo(LaneInfo_) {}
SIMD128IntUnary::~SIMD128IntUnary() noexcept = default;

SIMD128IntUnaryOperator SIMD128IntUnary::getOperator() const {
  return Operator;
}

void SIMD128IntUnary::setOperator(SIMD128IntUnaryOperator Operator_) {
  Operator = Operator_;
}

SIMD128IntLaneInfo SIMD128IntUnary::getLaneInfo() const { return LaneInfo; }

void SIMD128IntUnary::setLaneInfo(SIMD128IntLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128IntUnary::classof(Unary const *Inst) {
  return Inst->isSIMD128IntUnary();
}

bool SIMD128IntUnary::classof(Instruction const *Inst) {
  if (Unary::classof(Inst))
    return SIMD128IntUnary::classof(dyn_cast<Unary>(Inst));
  return false;
}

bool SIMD128IntUnary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128IntUnary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::unary

namespace fmt {
char const *
formatter<mir::instructions::unary::SIMD128IntUnaryOperator>::toString(
    UnaryOperator const &Operator) {
  // clang-format off
  switch (Operator) {
  case UnaryOperator::Neg    : return "v128.int.neg";
  case UnaryOperator::Abs    : return "v128.int.abs";
  case UnaryOperator::AllTrue: return "v128.int.alltrue";
  case UnaryOperator::Bitmask: return "v128.int.bitmask";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt

namespace mir::instructions::unary {
SIMD128FPUnary::SIMD128FPUnary(
    SIMD128FPUnaryOperator Operator_, SIMD128FPLaneInfo LaneInfo_,
    Instruction *Operand_)
    : Unary(UnaryKind::SIMD128FPUnary, Operand_), Operator(Operator_),
      LaneInfo(LaneInfo_) {}
SIMD128FPUnary::~SIMD128FPUnary() noexcept = default;

SIMD128FPUnaryOperator SIMD128FPUnary::getOperator() const { return Operator; }

void SIMD128FPUnary::setOperator(SIMD128FPUnaryOperator Operator_) {
  Operator = Operator_;
}

SIMD128FPLaneInfo SIMD128FPUnary::getLaneInfo() const { return LaneInfo; }

void SIMD128FPUnary::setLaneInfo(SIMD128FPLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128FPUnary::classof(Unary const *Inst) {
  return Inst->isSIMD128FPUnary();
}

bool SIMD128FPUnary::classof(Instruction const *Inst) {
  if (Unary::classof(Inst))
    return SIMD128FPUnary::classof(dyn_cast<Unary>(Inst));
  return false;
}

bool SIMD128FPUnary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128FPUnary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::unary

namespace fmt {
char const *
formatter<mir::instructions::unary::SIMD128FPUnaryOperator>::toString(
    UnaryOperator const &Operator) {
  // clang-format off
  switch (Operator) {
  case UnaryOperator::Neg    : return "v128.fp.neg";
  case UnaryOperator::Abs    : return "v128.fp.abs";
  case UnaryOperator::Sqrt   : return "v128.fp.sqrt";
  case UnaryOperator::Ceil   : return "v128.fp.ceil";
  case UnaryOperator::Floor  : return "v128.fp.floor";
  case UnaryOperator::Trunc  : return "v128.fp.trunc";
  case UnaryOperator::Nearest: return "v128.fp.nearest";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt