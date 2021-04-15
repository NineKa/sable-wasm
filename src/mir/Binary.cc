#include "Binary.h"

namespace mir::instructions {
Binary::Binary(BinaryKind Kind_, Instruction *LHS_, Instruction *RHS_)
    : Instruction(InstructionKind::Binary), Kind(Kind_), LHS(), RHS() {
  setLHS(LHS_);
  setRHS(RHS_);
}

Binary::~Binary() noexcept {
  if (LHS != nullptr) LHS->remove_use(this);
  if (RHS != nullptr) RHS->remove_use(this);
}

BinaryKind Binary::getBinaryKind() const { return Kind; }
bool Binary::isIntBinary() const { return Kind == BinaryKind::IntBinary; }
bool Binary::isFPBinary() const { return Kind == BinaryKind::FPBinary; }

bool Binary::isSIMD128Binary() const {
  return Kind == BinaryKind::SIMD128Binary;
}

bool Binary::isSIMD128IntBinary() const {
  return Kind == BinaryKind::SIMD128IntBinary;
}

bool Binary::isSIMD128FPBinary() const {
  return Kind == BinaryKind::SIMD128FPBinary;
}

Instruction *Binary::getLHS() const { return LHS; }
Instruction *Binary::getRHS() const { return RHS; }

void Binary::setLHS(Instruction *LHS_) {
  if (LHS != nullptr) LHS->remove_use(this);
  if (LHS_ != nullptr) LHS_->add_use(this);
  LHS = LHS_;
}

void Binary::setRHS(Instruction *RHS_) {
  if (RHS != nullptr) RHS->remove_use(this);
  if (RHS_ != nullptr) RHS_->add_use(this);
  RHS = RHS_;
}

void Binary::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getLHS() == Old) setLHS(dyn_cast<Instruction>(New));
  if (getRHS() == Old) setRHS(dyn_cast<Instruction>(New));
}

bool Binary::classof(Instruction const *Inst) {
  return Inst->getInstructionKind() == InstructionKind::Binary;
}

bool Binary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Binary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions

namespace mir::instructions::binary {
IntBinary::IntBinary(
    IntBinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_)
    : Binary(BinaryKind::IntBinary, LHS_, RHS_), Operator(Operator_) {}
IntBinary::~IntBinary() noexcept = default;

IntBinaryOperator IntBinary::getOperator() const { return Operator; }
void IntBinary::setOperator(IntBinaryOperator Operator_) {
  Operator = Operator_;
}

bool IntBinary::classof(Binary const *Inst) { return Inst->isIntBinary(); }

bool IntBinary::classof(Instruction const *Inst) {
  if (Binary::classof(Inst)) return IntBinary::classof(dyn_cast<Binary>(Inst));
  return false;
}
} // namespace mir::instructions::binary

namespace fmt {
char const *formatter<mir::instructions::binary::IntBinaryOperator>::toString(
    BinaryOperator const &Operator) {
  // clang-format off
  switch (Operator) {
  case BinaryOperator::Add : return "int.add";
  case BinaryOperator::Sub : return "int.sub";
  case BinaryOperator::Mul : return "int.mul";
  case BinaryOperator::DivS: return "int.div.s";
  case BinaryOperator::DivU: return "int.div.u";
  case BinaryOperator::RemS: return "int.rem.s";
  case BinaryOperator::RemU: return "int.rem.u";
  case BinaryOperator::And : return "int.and";
  case BinaryOperator::Or  : return "int.or";
  case BinaryOperator::Xor : return "int.xor";
  case BinaryOperator::Shl : return "int.shl";
  case BinaryOperator::ShrS: return "int.shr.s";
  case BinaryOperator::ShrU: return "int.shr.u";
  case BinaryOperator::Rotl: return "int.rotl";
  case BinaryOperator::Rotr: return "int.rotr";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt

namespace mir::instructions::binary {
bool IntBinary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return IntBinary::classof(dyn_cast<Instruction>(Node));
  return false;
}

FPBinary::FPBinary(
    FPBinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_)
    : Binary(BinaryKind::FPBinary, LHS_, RHS_), Operator(Operator_) {}
FPBinary::~FPBinary() noexcept = default;

FPBinaryOperator FPBinary::getOperator() const { return Operator; }
void FPBinary::setOperator(FPBinaryOperator Operator_) { Operator = Operator_; }

bool FPBinary::classof(Binary const *Inst) { return Inst->isFPBinary(); }

bool FPBinary::classof(Instruction const *Inst) {
  if (Binary::classof(Inst)) return FPBinary::classof(dyn_cast<Binary>(Inst));
  return false;
}

bool FPBinary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return FPBinary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::binary

namespace fmt {
char const *formatter<mir::instructions::binary::FPBinaryOperator>::toString(
    BinaryOperator const &Operator) {
  switch (Operator) {
  case BinaryOperator::Add: return "fp.add";
  case BinaryOperator::Sub: return "fp.sub";
  case BinaryOperator::Mul: return "fp.mul";
  case BinaryOperator::Div: return "fp.div";
  case BinaryOperator::Min: return "fp.min";
  case BinaryOperator::Max: return "fp.max";
  case BinaryOperator::CopySign: return "fp.copysign";
  default: utility::unreachable();
  }
}
} // namespace fmt

namespace mir::instructions::binary {
SIMD128Binary::SIMD128Binary(
    SIMD128BinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_)
    : Binary(BinaryKind::SIMD128Binary, LHS_, RHS_), Operator(Operator_) {}
SIMD128Binary::~SIMD128Binary() noexcept = default;

SIMD128BinaryOperator SIMD128Binary::getOperator() const { return Operator; }
void SIMD128Binary::setOperator(SIMD128BinaryOperator Operator_) {
  Operator = Operator_;
}

bool SIMD128Binary::classof(Binary const *Inst) {
  return Inst->isSIMD128Binary();
}

bool SIMD128Binary::classof(Instruction const *Inst) {
  if (Binary::classof(Inst))
    return SIMD128Binary::classof(dyn_cast<Binary>(Inst));
  return false;
}

bool SIMD128Binary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128Binary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::binary

namespace fmt {
char const *
formatter<mir::instructions::binary::SIMD128BinaryOperator>::toString(
    BinaryOperator const &Operator) {
  switch (Operator) {
  case BinaryOperator::And: return "v128.and";
  case BinaryOperator::Or: return "v128.or";
  case BinaryOperator::Xor: return "v128.xor";
  case BinaryOperator::AndNot: return "v128.andnot";
  default: utility::unreachable();
  }
}
} // namespace fmt

namespace mir::instructions::binary {
SIMD128IntBinary::SIMD128IntBinary(
    SIMD128IntBinaryOperator Operator_, SIMD128IntLaneInfo LaneInfo_,
    Instruction *LHS_, Instruction *RHS_)
    : Binary(BinaryKind::SIMD128IntBinary, LHS_, RHS_), Operator(Operator_),
      LaneInfo(LaneInfo_) {}
SIMD128IntBinary::~SIMD128IntBinary() noexcept = default;

SIMD128IntBinaryOperator SIMD128IntBinary::getOperator() const {
  return Operator;
}

void SIMD128IntBinary::setOperator(SIMD128IntBinaryOperator Operator_) {
  Operator = Operator_;
}

SIMD128IntLaneInfo SIMD128IntBinary::getLaneInfo() const { return LaneInfo; }

void SIMD128IntBinary::setLaneInfo(SIMD128IntLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128IntBinary::classof(Binary const *Inst) {
  return Inst->isSIMD128IntBinary();
}

bool SIMD128IntBinary::classof(Instruction const *Inst) {
  if (Binary::classof(Inst))
    return SIMD128IntBinary::classof(dyn_cast<Binary>(Inst));
  return false;
}

bool SIMD128IntBinary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128IntBinary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::binary

namespace fmt {
char const *
formatter<mir::instructions::binary::SIMD128IntBinaryOperator>::toString(
    BinaryOperator const &Operator) {
  // clang-format off
  switch (Operator) {
  case BinaryOperator::Add            : return "v128.int.add";
  case BinaryOperator::Sub            : return "v128.int.sub";
  case BinaryOperator::Mul            : return "v128.int.mul";
  case BinaryOperator::ExtMulLowS     : return "v128.int.ext.mul.low.s";
  case BinaryOperator::ExtMulLowU     : return "v128.int.ext.mul.low.u";
  case BinaryOperator::ExtMulHighS    : return "v128.int.ext.mul.high.s";
  case BinaryOperator::ExtMulHighU    : return "v128.int.ext.mul.high.u";
  case BinaryOperator::ExtAddPairwiseS: return "v128.int.ext.add.pairwise.s";
  case BinaryOperator::ExtAddPairwiseU: return "v128.int.ext.add.pairwise.u";
  case BinaryOperator::AddSatS        : return "v128.int.add.sat.s";
  case BinaryOperator::AddSatU        : return "v128.int.add.sat.u";
  case BinaryOperator::SubSatS        : return "v128.int.sub.sat.s";
  case BinaryOperator::SubSatU        : return "v128.int.sub.sat.u";
  case BinaryOperator::Shl            : return "v128.int.shl";
  case BinaryOperator::ShrS           : return "v128.int.shr.s";
  case BinaryOperator::ShrU           : return "v128.int.shr.u";
  case BinaryOperator::MinS           : return "v128.int.min.s";
  case BinaryOperator::MinU           : return "v128.int.min.u";
  case BinaryOperator::MaxS           : return "v128.int.max.s";
  case BinaryOperator::MaxU           : return "v128.int.max.u";
  case BinaryOperator::AvgrU          : return "v128.int.avgr.u";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt

namespace mir::instructions::binary {
SIMD128FPBinary::SIMD128FPBinary(
    SIMD128FPBinaryOperator Operator_, SIMD128FPLaneInfo LaneInfo_,
    Instruction *LHS_, Instruction *RHS_)
    : Binary(BinaryKind::SIMD128FPBinary, LHS_, RHS_), Operator(Operator_),
      LaneInfo(LaneInfo_) {}
SIMD128FPBinary::~SIMD128FPBinary() noexcept = default;

SIMD128FPBinaryOperator SIMD128FPBinary::getOperator() const {
  return Operator;
}

void SIMD128FPBinary::setOperator(SIMD128FPBinaryOperator Operator_) {
  Operator = Operator_;
}

SIMD128FPLaneInfo SIMD128FPBinary::getLaneInfo() const { return LaneInfo; }

void SIMD128FPBinary::setLaneInfo(SIMD128FPLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128FPBinary::classof(Binary const *Inst) {
  return Inst->isSIMD128FPBinary();
}

bool SIMD128FPBinary::classof(Instruction const *Inst) {
  if (Binary::classof(Inst))
    return SIMD128FPBinary::classof(dyn_cast<Binary>(Inst));
  return false;
}

bool SIMD128FPBinary::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128FPBinary::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::binary

namespace fmt {
char const *
formatter<mir::instructions::binary::SIMD128FPBinaryOperator>::toString(
    BinaryOperator const &Operator) {
  // clang-format off
  switch (Operator) {
  case BinaryOperator::Add : return "v128.fp.add";
  case BinaryOperator::Sub : return "v128.fp.sub";
  case BinaryOperator::Div : return "v128.fp.div";
  case BinaryOperator::Mul : return "v128.fp.mul";
  case BinaryOperator::Min : return "v128.fp.min";
  case BinaryOperator::Max : return "v128.fp.max";
  case BinaryOperator::PMin: return "v128.fp.pmin";
  case BinaryOperator::PMax: return "v128.fp.pmax";
  default: utility::unreachable();
  }
  //  clang-format on
}
} // namespace fmt