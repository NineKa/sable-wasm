#include "Compare.h"

namespace mir::instructions {
Compare::Compare(CompareKind Kind_, Instruction *LHS_, Instruction *RHS_)
    : Instruction(InstructionKind::Compare), Kind(Kind_), LHS(), RHS() {
  setLHS(LHS_);
  setRHS(RHS_);
}

Compare::~Compare() noexcept {
  if (LHS != nullptr) LHS->remove_use(this);
  if (RHS != nullptr) RHS->remove_use(this);
}

CompareKind Compare::getCompareKind() const { return Kind; }
bool Compare::isIntCompare() const { return Kind == CompareKind::IntCompare; }
bool Compare::isFPCompare() const { return Kind == CompareKind::FPCompare; }

bool Compare::isSIMD128IntCompare() const {
  return Kind == CompareKind::SIMD128IntCompare;
}

bool Compare::isSIMD128FPCompare() const {
  return Kind == CompareKind::SIMD128FPCompare;
}

Instruction *Compare::getLHS() const { return LHS; }
Instruction *Compare::getRHS() const { return RHS; }

void Compare::setLHS(Instruction *LHS_) {
  if (LHS != nullptr) LHS->remove_use(this);
  if (LHS_ != nullptr) LHS_->add_use(this);
  LHS = LHS_;
}

void Compare::setRHS(Instruction *RHS_) {
  if (RHS != nullptr) RHS->remove_use(this);
  if (RHS_ != nullptr) RHS_->add_use(this);
  RHS = RHS_;
}

void Compare::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getLHS() == Old) setLHS(dyn_cast<Instruction>(New));
  if (getRHS() == Old) setRHS(dyn_cast<Instruction>(New));
}

bool Compare::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == InstructionKind::Compare;
}

bool Compare::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Compare::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions

namespace mir::instructions::compare {
IntCompare::IntCompare(
    IntCompareOperator Operator_, Instruction *LHS_, Instruction *RHS_)
    : Compare(CompareKind::IntCompare, LHS_, RHS_), Operator(Operator_) {}
IntCompare::~IntCompare() noexcept = default;

IntCompareOperator IntCompare::getOperator() const { return Operator; }
void IntCompare::setOperator(IntCompareOperator Operator_) {
  Operator = Operator_;
}

bool IntCompare::classof(Compare const *Inst) { return Inst->isIntCompare(); }

bool IntCompare::classof(Instruction const *Inst) {
  if (Compare::classof(Inst))
    return IntCompare::classof(dyn_cast<Compare>(Inst));
  return false;
}

bool IntCompare::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return IntCompare::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::compare

namespace fmt {
using namespace mir::instructions::compare;
char const *
formatter<IntCompareOperator>::toString(IntCompareOperator const &Operator) {
  // clang-format off
  switch (Operator) {
  case IntCompareOperator::Eq : return "int.eq"  ;
  case IntCompareOperator::Ne : return "int.ne"  ;
  case IntCompareOperator::LtS: return "int.lt.s";
  case IntCompareOperator::LtU: return "int.lt.u";
  case IntCompareOperator::GtS: return "int.gt.s";
  case IntCompareOperator::GtU: return "int.gt.u";
  case IntCompareOperator::LeS: return "int.le.s";
  case IntCompareOperator::LeU: return "int.le.u";
  case IntCompareOperator::GeS: return "int.ge.s";
  case IntCompareOperator::GeU: return "int.ge.u";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt

namespace mir::instructions::compare {
FPCompare::FPCompare(
    FPCompareOperator Operator_, Instruction *LHS_, Instruction *RHS_)
    : Compare(CompareKind::FPCompare, LHS_, RHS_), Operator(Operator_) {}
FPCompare::~FPCompare() noexcept = default;

FPCompareOperator FPCompare::getOperator() const { return Operator; }
void FPCompare::setOperator(FPCompareOperator Operator_) {
  Operator = Operator_;
}

bool FPCompare::classof(Compare const *Inst) { return Inst->isFPCompare(); }

bool FPCompare::classof(Instruction const *Inst) {
  if (Compare::classof(Inst))
    return FPCompare::classof(dyn_cast<Compare>(Inst));
  return false;
}

bool FPCompare::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return FPCompare::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::compare

namespace fmt {
char const *
formatter<FPCompareOperator>::toString(FPCompareOperator const &Operator) {
  switch (Operator) {
  case FPCompareOperator::Eq: return "fp.eq";
  case FPCompareOperator::Ne: return "fp.ne";
  case FPCompareOperator::Lt: return "fp.lt";
  case FPCompareOperator::Gt: return "fp.gt";
  case FPCompareOperator::Le: return "fp.le";
  case FPCompareOperator::Ge: return "fp.ge";
  default: utility::unreachable();
  }
}
} // namespace fmt

namespace mir::instructions::compare {
SIMD128IntCompare::SIMD128IntCompare(
    SIMD128IntCompareOperator Operator_, SIMD128IntLaneInfo LaneInfo_,
    Instruction *LHS_, Instruction *RHS_)
    : Compare(CompareKind::SIMD128IntCompare, LHS_, RHS_), Operator(Operator_),
      LaneInfo(LaneInfo_) {}

SIMD128IntCompare::~SIMD128IntCompare() noexcept = default;

SIMD128IntCompareOperator SIMD128IntCompare::getOperator() const {
  return Operator;
}

void SIMD128IntCompare::setOperator(SIMD128IntCompareOperator Operator_) {
  Operator = Operator_;
}

SIMD128IntLaneInfo SIMD128IntCompare::getLaneInfo() const { return LaneInfo; }

void SIMD128IntCompare::setLaneInfo(SIMD128IntLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128IntCompare::classof(Compare const *Inst) {
  return Inst->isSIMD128IntCompare();
}

bool SIMD128IntCompare::classof(Instruction const *Inst) {
  if (Compare::classof(Inst))
    return SIMD128IntCompare::classof(dyn_cast<Compare>(Inst));
  return false;
}

bool SIMD128IntCompare::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128IntCompare::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::compare

namespace fmt {
char const *formatter<SIMD128IntCompareOperator>::toString(
    CompareOperator const &Operator) {
  // clang-format off
  switch (Operator) {
  case SIMD128IntCompareOperator::Eq : return "v128.int.eq"  ;
  case SIMD128IntCompareOperator::Ne : return "v128.int.ne"  ;
  case SIMD128IntCompareOperator::LtS: return "v128.int.lt.s";
  case SIMD128IntCompareOperator::LtU: return "v128.int.lt.u";
  case SIMD128IntCompareOperator::GtS: return "v128.int.gt.s";
  case SIMD128IntCompareOperator::GtU: return "v128.int.gt.u";
  case SIMD128IntCompareOperator::LeS: return "v128.int.le.s";
  case SIMD128IntCompareOperator::LeU: return "v128.int.le.u";
  case SIMD128IntCompareOperator::GeS: return "v128.int.ge.s";
  case SIMD128IntCompareOperator::GeU: return "v128.int.ge.u";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt

namespace mir::instructions::compare {
SIMD128FPCompare::SIMD128FPCompare(
    SIMD128FPCompareOperator Operator_, SIMD128FPLaneInfo LaneInfo_,
    Instruction *LHS_, Instruction *RHS_)
    : Compare(CompareKind::SIMD128FPCompare, LHS_, RHS_), Operator(Operator_),
      LaneInfo(LaneInfo_) {}

SIMD128FPCompare::~SIMD128FPCompare() noexcept = default;

SIMD128FPCompareOperator SIMD128FPCompare::getOperator() const {
  return Operator;
}

void SIMD128FPCompare::setOperator(SIMD128FPCompareOperator Operator_) {
  Operator = Operator_;
}

SIMD128FPLaneInfo SIMD128FPCompare::getLaneInfo() const { return LaneInfo; }

void SIMD128FPCompare::setLaneInfo(SIMD128FPLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128FPCompare::classof(Compare const *Inst) {
  return Inst->isSIMD128FPCompare();
}

bool SIMD128FPCompare::classof(Instruction const *Inst) {
  if (Compare::classof(Inst))
    return SIMD128FPCompare::classof(dyn_cast<Compare>(Inst));
  return false;
}

bool SIMD128FPCompare::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128FPCompare::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::compare

namespace fmt {
char const *
formatter<SIMD128FPCompareOperator>::toString(CompareOperator const &Operator) {
  switch (Operator) {
  case SIMD128FPCompareOperator::Eq: return "v128.fp.eq";
  case SIMD128FPCompareOperator::Ne: return "v128.fp.ne";
  case SIMD128FPCompareOperator::Lt: return "v128.fp.lt";
  case SIMD128FPCompareOperator::Gt: return "v128.fp.gt";
  case SIMD128FPCompareOperator::Le: return "v128.fp.le";
  case SIMD128FPCompareOperator::Ge: return "v128.fp.ge";
  default: utility::unreachable();
  }
}
} // namespace fmt