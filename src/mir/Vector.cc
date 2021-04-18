#include "Vector.h"
#include "ASTNode.h"
#include "Instruction.h"

#include <range/v3/algorithm/copy.hpp>

namespace mir::instructions {
VectorSplat::VectorSplat(VectorSplatKind Kind_, mir::Instruction *Operand_)
    : Instruction(InstructionKind::VectorSplat), Kind(Kind_), Operand() {
  setOperand(Operand_);
}

VectorSplat::~VectorSplat() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

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

namespace mir::instructions {
VectorExtract::VectorExtract(
    VectorExtractKind Kind_, mir::Instruction *Operand_, unsigned LaneIndex_)
    : Instruction(InstructionKind::VectorExtract), Kind(Kind_), Operand(),
      LaneIndex(LaneIndex_) {
  setOperand(Operand_);
}

VectorExtract::~VectorExtract() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

VectorExtractKind VectorExtract::getVectorExtractKind() const { return Kind; }

bool VectorExtract::isSIMD128IntExtract() const {
  return Kind == VectorExtractKind::SIMD128IntExtract;
}

bool VectorExtract::isSIMD128FPExtract() const {
  return Kind == VectorExtractKind::SIMD128FPExtract;
}

mir::Instruction *VectorExtract::getOperand() const { return Operand; }

void VectorExtract::setOperand(mir::Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

unsigned VectorExtract::getLaneIndex() const { return LaneIndex; }

void VectorExtract::setLaneIndex(unsigned LaneIndex_) {
  LaneIndex = LaneIndex_;
}

void VectorExtract::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

bool VectorExtract::classof(mir::Instruction const *Inst) {
  return Inst->getInstructionKind() == InstructionKind::VectorExtract;
}

bool VectorExtract::classof(mir::ASTNode const *Node) {
  if (Instruction::classof(Node))
    return VectorExtract::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions

namespace mir::instructions::vector_extract {
SIMD128IntExtract::SIMD128IntExtract(
    SIMD128IntLaneInfo LaneInfo_, mir::Instruction *Operand_,
    unsigned LaneIndex_)
    : VectorExtract(VectorExtractKind::SIMD128IntExtract, Operand_, LaneIndex_),
      LaneInfo(LaneInfo_) {}

SIMD128IntExtract::~SIMD128IntExtract() noexcept = default;

SIMD128IntLaneInfo SIMD128IntExtract::getLaneInfo() const { return LaneInfo; }

void SIMD128IntExtract::setLaneInfo(SIMD128IntLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128IntExtract::classof(VectorExtract const *Inst) {
  return Inst->isSIMD128IntExtract();
}

bool SIMD128IntExtract::classof(mir::Instruction const *Inst) {
  if (VectorExtract::classof(Inst))
    return SIMD128IntExtract::classof(dyn_cast<VectorExtract>(Inst));
  return false;
}

bool SIMD128IntExtract::classof(mir::ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128IntExtract::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::vector_extract

namespace mir::instructions::vector_extract {
SIMD128FPExtract::SIMD128FPExtract(
    SIMD128FPLaneInfo LaneInfo_, mir::Instruction *Operand_,
    unsigned LaneIndex_)
    : VectorExtract(VectorExtractKind::SIMD128FPExtract, Operand_, LaneIndex_),
      LaneInfo(LaneInfo_) {}

SIMD128FPExtract::~SIMD128FPExtract() noexcept = default;

SIMD128FPLaneInfo SIMD128FPExtract::getLaneInfo() const { return LaneInfo; }

void SIMD128FPExtract::setLaneInfo(SIMD128FPLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128FPExtract::classof(VectorExtract const *Inst) {
  return Inst->isSIMD128FPExtract();
}

bool SIMD128FPExtract::classof(mir::Instruction const *Inst) {
  if (VectorExtract::classof(Inst))
    return SIMD128FPExtract::classof(dyn_cast<VectorExtract>(Inst));
  return false;
}

bool SIMD128FPExtract::classof(mir::ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128FPExtract::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::vector_extract

namespace mir::instructions {
VectorInsert::VectorInsert(
    VectorInsertKind Kind_, mir::Instruction *TargetVector_,
    unsigned LaneIndex_, mir::Instruction *CandidateValue_)
    : Instruction(InstructionKind::VectorInsert), Kind(Kind_), TargetVector(),
      CandidateValue(), LaneIndex(LaneIndex_) {
  setTargetVector(TargetVector_);
  setCandidateValue(CandidateValue_);
}

VectorInsert::~VectorInsert() noexcept {
  if (TargetVector != nullptr) TargetVector->remove_use(this);
  if (CandidateValue != nullptr) CandidateValue->remove_use(this);
}

VectorInsertKind VectorInsert::getVectorInsertKind() const { return Kind; }

bool VectorInsert::isSIMD128IntInsert() const {
  return Kind == VectorInsertKind::SIMD128IntInsert;
}

bool VectorInsert::isSIMD128FPInsert() const {
  return Kind == VectorInsertKind::SIMD128FPInsert;
}

mir::Instruction *VectorInsert::getTargetVector() const { return TargetVector; }

void VectorInsert::setTargetVector(mir::Instruction *TargetVector_) {
  if (TargetVector != nullptr) TargetVector->remove_use(this);
  if (TargetVector_ != nullptr) TargetVector_->add_use(this);
  TargetVector = TargetVector_;
}

mir::Instruction *VectorInsert::getCandidateValue() const {
  return CandidateValue;
}

void VectorInsert::setCandidateValue(mir::Instruction *CandidateValue_) {
  if (CandidateValue != nullptr) CandidateValue->remove_use(this);
  if (CandidateValue_ != nullptr) CandidateValue_->add_use(this);
  CandidateValue = CandidateValue_;
}

void VectorInsert::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getTargetVector() == Old) setTargetVector(dyn_cast<Instruction>(New));
  if (getCandidateValue() == Old) setCandidateValue(dyn_cast<Instruction>(New));
}

unsigned VectorInsert::getLaneIndex() const { return LaneIndex; }

void VectorInsert::setLaneIndex(unsigned LaneIndex_) { LaneIndex = LaneIndex_; }

bool VectorInsert::classof(mir::Instruction const *Inst) {
  return Inst->getInstructionKind() == InstructionKind::VectorInsert;
}

bool VectorInsert::classof(mir::ASTNode const *Node) {
  if (Instruction::classof(Node))
    return VectorInsert::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions

namespace mir::instructions::vector_insert {
SIMD128IntInsert::SIMD128IntInsert(
    SIMD128IntLaneInfo LaneInfo_, mir::Instruction *TargetVector_,
    unsigned LaneIndex_, mir::Instruction *CandidateValue_)
    : VectorInsert(
          VectorInsertKind::SIMD128IntInsert, TargetVector_, LaneIndex_,
          CandidateValue_),
      LaneInfo(LaneInfo_) {}

SIMD128IntInsert::~SIMD128IntInsert() noexcept = default;

SIMD128IntLaneInfo SIMD128IntInsert::getLaneInfo() const { return LaneInfo; }

void SIMD128IntInsert::setLaneInfo(SIMD128IntLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128IntInsert::classof(VectorInsert const *Inst) {
  return Inst->isSIMD128IntInsert();
}

bool SIMD128IntInsert::classof(mir::Instruction const *Inst) {
  if (VectorInsert::classof(Inst))
    return SIMD128IntInsert::classof(dyn_cast<VectorInsert>(Inst));
  return false;
}

bool SIMD128IntInsert::classof(mir::ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128IntInsert::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::vector_insert

namespace mir::instructions::vector_insert {
SIMD128FPInsert::SIMD128FPInsert(
    SIMD128FPLaneInfo LaneInfo_, mir::Instruction *TargetVector_,
    unsigned LaneIndex_, mir::Instruction *CandidateValue_)
    : VectorInsert(
          VectorInsertKind::SIMD128FPInsert, TargetVector_, LaneIndex_,
          CandidateValue_),
      LaneInfo(LaneInfo_) {}

SIMD128FPInsert::~SIMD128FPInsert() noexcept = default;

SIMD128FPLaneInfo SIMD128FPInsert::getLaneInfo() const { return LaneInfo; }

void SIMD128FPInsert::setLaneInfo(SIMD128FPLaneInfo LaneInfo_) {
  LaneInfo = LaneInfo_;
}

bool SIMD128FPInsert::classof(VectorInsert const *Inst) {
  return Inst->isSIMD128FPInsert();
}

bool SIMD128FPInsert::classof(Instruction const *Inst) {
  if (VectorInsert::classof(Inst))
    return SIMD128FPInsert::classof(dyn_cast<VectorInsert>(Inst));
  return false;
}

bool SIMD128FPInsert::classof(ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128FPInsert::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions::vector_insert

namespace mir::instructions {
SIMD128ShuffleByte::SIMD128ShuffleByte(
    mir::Instruction *Low_, mir::Instruction *High_,
    std::span<unsigned const, 16> Mask_)
    : Instruction(InstructionKind::SIMD128ShuffleByte), Low(), High(), Mask() {
  setLow(Low_);
  setHigh(High_);
  ranges::copy(Mask_.begin(), Mask_.end(), Mask.begin());
}

SIMD128ShuffleByte::~SIMD128ShuffleByte() noexcept {
  if (Low != nullptr) Low->remove_use(this);
  if (High != nullptr) High->remove_use(this);
}

mir::Instruction *SIMD128ShuffleByte::getLow() const { return Low; }
mir::Instruction *SIMD128ShuffleByte::getHigh() const { return High; }

void SIMD128ShuffleByte::setLow(mir::Instruction *Low_) {
  if (Low != nullptr) Low->remove_use(this);
  if (Low_ != nullptr) Low_->add_use(this);
  Low = Low_;
}

void SIMD128ShuffleByte::setHigh(mir::Instruction *High_) {
  if (High != nullptr) High->remove_use(this);
  if (High_ != nullptr) High_->add_use(this);
  High = High_;
}

std::span<unsigned const, 16> SIMD128ShuffleByte::getMask() const {
  return Mask;
}

void SIMD128ShuffleByte::setMask(std::span<const unsigned int, 16> Mask_) {
  ranges::copy(Mask_.begin(), Mask_.end(), Mask.begin());
}

void SIMD128ShuffleByte::replace(
    mir::ASTNode const *Old, ASTNode *New) noexcept {
  if (getLow() == Old) setLow(dyn_cast<Instruction>(New));
  if (getHigh() == Old) setHigh(dyn_cast<Instruction>(New));
}

bool SIMD128ShuffleByte::classof(mir::Instruction const *Inst) {
  return Inst->getInstructionKind() == InstructionKind::SIMD128ShuffleByte;
}

bool SIMD128ShuffleByte::classof(mir::ASTNode const *Node) {
  if (Instruction::classof(Node))
    return SIMD128ShuffleByte::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions