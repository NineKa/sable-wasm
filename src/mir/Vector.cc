#include "Vector.h"

namespace mir::instructions {
VectorSplat::VectorSplat(VectorSplatKind Kind_, mir::Instruction *Operand_)
    : Instruction(InstructionKind::VectorSplat), Kind(Kind_), Operand() {
  setOperand(Operand_);
}
VectorSplat::~VectorSplat() noexcept = default;

mir::Instruction *VectorSplat::getOperand() const { return Operand; }

void VectorSplat::setOperand(mir::Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

VectorSplatKind VectorSplat::getVectorSplatKind() const { return Kind; }

bool VectorSplat::isSIMD128IntSplat() const {
  return Kind == VectorSplatKind::SIMD128IntSplat;
}

bool VectorSplat::isSIMD128FPSplat() const {
  return Kind == VectorSplatKind::SIMD128FPSplat;
}

void VectorSplat::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool VectorSplat::classof(mir::Instruction const *Inst) {
  return Inst->getInstructionKind() == InstructionKind::VectorSplat;
}

bool VectorSplat::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return VectorSplat::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions

namespace mir::instructions::vector_splat {
SIMD128IntSplat::SIMD128IntSplat(
    SIMD128IntLaneInfo LaneInfo_, mir::Instruction *Operand_)
    : VectorSplat(VectorSplatKind::SIMD128IntSplat, Operand_),
      LaneInfo(LaneInfo_) {}
SIMD128IntSplat::~SIMD128IntSplat() noexcept = default;

SIMD128IntLaneInfo SIMD128IntSplat::getLaneInfo() const { return LaneInfo; }

void SIMD128IntSplat::setLaneInfo(SIMD128IntLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128IntSplat::classof(VectorSplat const *Inst) {
  return Inst->isSIMD128IntSplat();
}

bool SIMD128IntSplat::classof(mir::Instruction const *Inst) {
  if (VectorSplat::classof(Inst))
    return SIMD128IntSplat::classof(dyn_cast<VectorSplat>(Inst));
  return false;
}

bool SIMD128IntSplat::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return VectorSplat::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::vector_splat

namespace mir::instructions::vector_splat {
SIMD128FPSplat::SIMD128FPSplat(
    SIMD128FPLaneInfo LaneInfo_, mir::Instruction *Operand_)
    : VectorSplat(VectorSplatKind::SIMD128FPSplat, Operand_),
      LaneInfo(LaneInfo_) {}
SIMD128FPSplat::~SIMD128FPSplat() noexcept = default;

SIMD128FPLaneInfo SIMD128FPSplat::getLaneInfo() const { return LaneInfo; }

void SIMD128FPSplat::setLaneInfo(SIMD128FPLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128FPSplat::classof(VectorSplat const *Inst) {
  return Inst->isSIMD128FPSplat();
}

bool SIMD128FPSplat::classof(mir::Instruction const *Inst) {
  if (VectorSplat::classof(Inst))
    return SIMD128FPSplat::classof(dyn_cast<VectorSplat>(Inst));
  return false;
}

bool SIMD128FPSplat::classof(mir::ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128FPSplat::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::vector_splat