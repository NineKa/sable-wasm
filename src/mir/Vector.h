#ifndef SABLE_INCLUDE_GUARD_MIR_VECTOR
#define SABLE_INCLUDE_GUARD_MIR_VECTOR

#include "Instruction.h"

namespace mir::instructions {

enum class VectorSplatKind { SIMD128IntSplat, SIMD128FPSplat };
class VectorSplat : public Instruction {
  VectorSplatKind Kind;
  mir::Instruction *Operand;

public:
  VectorSplat(VectorSplatKind Kind_, mir::Instruction *Operand_);
  VectorSplat(VectorSplat const &) = delete;
  VectorSplat(VectorSplat &&) noexcept = delete;
  VectorSplat &operator=(VectorSplat const &) = delete;
  VectorSplat &operator=(VectorSplat &&) noexcept = delete;
  ~VectorSplat() noexcept override;

  mir::Instruction *getOperand() const;
  void setOperand(mir::Instruction *Operand_);

  VectorSplatKind getVectorSplatKind() const;
  bool isSIMD128IntSplat() const;
  bool isSIMD128FPSplat() const;

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};

SABLE_DEFINE_IS_A(VectorSplat)
SABLE_DEFINE_DYN_CAST(VectorSplat)

namespace vector_splat {
class SIMD128IntSplat : public VectorSplat {
  SIMD128IntLaneInfo LaneInfo;

public:
  SIMD128IntSplat(SIMD128IntLaneInfo LaneInfo_, mir::Instruction *Operand_);
  SIMD128IntSplat(SIMD128IntSplat const &) = delete;
  SIMD128IntSplat(SIMD128IntSplat &&) noexcept = delete;
  SIMD128IntSplat &operator=(SIMD128IntSplat const &) = delete;
  SIMD128IntSplat &operator=(SIMD128IntSplat &&) noexcept = delete;
  ~SIMD128IntSplat() noexcept override;

  SIMD128IntLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128IntLaneInfo LaneInfo_);

  static bool classof(VectorSplat const *Inst);
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};

class SIMD128FPSplat : public VectorSplat {
  SIMD128FPLaneInfo LaneInfo;

public:
  SIMD128FPSplat(SIMD128FPLaneInfo LaneInfo_, mir::Instruction *Operand_);
  SIMD128FPSplat(SIMD128FPSplat const &) = delete;
  SIMD128FPSplat(SIMD128FPSplat &&) noexcept = delete;
  SIMD128FPSplat &operator=(SIMD128FPSplat const &) = delete;
  SIMD128FPSplat &operator=(SIMD128FPSplat &&) noexcept = delete;
  ~SIMD128FPSplat() noexcept override;

  SIMD128FPLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128FPLaneInfo LaneInfo_);

  static bool classof(VectorSplat const *Inst);
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};
} // namespace vector_splat

enum class VectorExtractKind { SIMD128IntExtract, SIMD128FPExtract };
class VectorExtract : public Instruction {
  VectorExtractKind Kind;
  mir::Instruction *Operand;
  unsigned LaneIndex;

public:
  VectorExtract(
      VectorExtractKind Kind_, mir::Instruction *Operand_, unsigned LaneIndex_);
  VectorExtract(VectorExtract const &) = delete;
  VectorExtract(VectorExtract &&) noexcept = delete;
  VectorExtract &operator=(VectorExtract const &) = delete;
  VectorExtract &operator=(VectorExtract &&) noexcept = delete;
  ~VectorExtract() noexcept override;

  VectorExtractKind getVectorExtractKind() const;
  bool isSIMD128IntExtract() const;
  bool isSIMD128FPExtract() const;

  mir::Instruction *getOperand() const;
  void setOperand(mir::Instruction *Operand_);
  unsigned getLaneIndex() const;
  void setLaneIndex(unsigned LaneIndex_);

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};

SABLE_DEFINE_IS_A(VectorExtract)
SABLE_DEFINE_DYN_CAST(VectorExtract)

namespace vector_extract {
class SIMD128IntExtract : public VectorExtract {
  SIMD128IntLaneInfo LaneInfo;

public:
  SIMD128IntExtract(
      SIMD128IntLaneInfo LaneInfo_, mir::Instruction *Operand_,
      unsigned LaneIndex_);
  SIMD128IntExtract(SIMD128IntExtract const &) = delete;
  SIMD128IntExtract(SIMD128IntExtract &&) noexcept = delete;
  SIMD128IntExtract &operator=(SIMD128IntExtract const &) = delete;
  SIMD128IntExtract &operator=(SIMD128IntExtract &&) noexcept = delete;
  ~SIMD128IntExtract() noexcept override;

  SIMD128IntLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128IntLaneInfo LaneInfo_);

  static bool classof(VectorExtract const *Inst);
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};

class SIMD128FPExtract : public VectorExtract {
  SIMD128FPLaneInfo LaneInfo;

public:
  SIMD128FPExtract(
      SIMD128FPLaneInfo LaneInfo_, mir::Instruction *Operand_,
      unsigned LaneIndex_);
  SIMD128FPExtract(SIMD128FPExtract const &) = delete;
  SIMD128FPExtract(SIMD128FPExtract &&) noexcept = delete;
  SIMD128FPExtract &operator=(SIMD128FPExtract const &) = delete;
  SIMD128FPExtract &operator=(SIMD128FPExtract &&) noexcept = delete;
  ~SIMD128FPExtract() noexcept override;

  SIMD128FPLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128FPLaneInfo LaneInfo_);

  static bool classof(VectorExtract const *Inst);
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};
} // namespace vector_extract

enum class VectorInsertKind { SIMD128IntInsert, SIMD128FPInsert };
class VectorInsert : public Instruction {
  VectorInsertKind Kind;
  mir::Instruction *TargetVector;
  mir::Instruction *CandidateValue;
  unsigned LaneIndex;

public:
  VectorInsert(
      VectorInsertKind Kind_, mir::Instruction *TargetVector_,
      unsigned LaneIndex_, mir::Instruction *CandidateValue_);
  VectorInsert(VectorInsert const &) = delete;
  VectorInsert(VectorInsert &&) noexcept = delete;
  VectorInsert &operator=(VectorInsert const &) = delete;
  VectorInsert &operator=(VectorInsert &&) noexcept = delete;
  ~VectorInsert() noexcept override;

  VectorInsertKind getVectorInsertKind() const;
  bool isSIMD128IntInsert() const;
  bool isSIMD128FPInsert() const;

  mir::Instruction *getTargetVector() const;
  void setTargetVector(mir::Instruction *TargetVector_);
  mir::Instruction *getCandidateValue() const;
  void setCandidateValue(mir::Instruction *CandidateValue_);
  unsigned getLaneIndex() const;
  void setLaneIndex(unsigned LaneIndex_);

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};

namespace vector_insert {
class SIMD128IntInsert : public VectorInsert {
  SIMD128IntLaneInfo LaneInfo;

public:
  SIMD128IntInsert(
      SIMD128IntLaneInfo LaneInfo_, mir::Instruction *TargetVector_,
      unsigned LaneIndex_, mir::Instruction *CandidateValue_);
  SIMD128IntInsert(SIMD128IntInsert const &) = delete;
  SIMD128IntInsert(SIMD128IntInsert &&) noexcept = delete;
  SIMD128IntInsert &operator=(SIMD128IntInsert const &) = delete;
  SIMD128IntInsert &operator=(SIMD128IntInsert &&) noexcept = delete;
  ~SIMD128IntInsert() noexcept override;

  SIMD128IntLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128IntLaneInfo LaneInfo_);

  static bool classof(VectorInsert const *Inst);
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};

class SIMD128FPInsert : public VectorInsert {
  SIMD128FPLaneInfo LaneInfo;

public:
  SIMD128FPInsert(
      SIMD128FPLaneInfo LaneInfo_, mir::Instruction *TargetVector_,
      unsigned LaneIndex_, mir::Instruction *CandidateValue_);
  SIMD128FPInsert(SIMD128FPInsert const &) = delete;
  SIMD128FPInsert(SIMD128FPInsert &&) noexcept = delete;
  SIMD128FPInsert &operator=(SIMD128FPInsert const &) = delete;
  SIMD128FPInsert &operator=(SIMD128FPInsert &&) noexcept = delete;
  ~SIMD128FPInsert() noexcept override;

  SIMD128FPLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128FPLaneInfo LaneInfo_);

  static bool classof(VectorInsert const *Inst);
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};
} // namespace vector_insert

template <typename Derived, typename RetType = void, bool Const = true>
class VectorSplatVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <typename T> RetType castAndCall(Ptr<VectorSplat> Inst) {
    return derived()(dyn_cast<T>(Inst));
  }

public:
  RetType visit(Ptr<VectorSplat> Inst) {
    using namespace vector_splat;
    using SKind = VectorSplatKind;
    switch (Inst->getVectorSplatKind()) {
    case SKind::SIMD128IntSplat: return castAndCall<SIMD128IntSplat>(Inst);
    case SKind::SIMD128FPSplat: return castAndCall<SIMD128FPSplat>(Inst);
    default: utility::unreachable();
    }
  }
};

template <typename Derived, typename RetType = void, bool Const = true>
class VectorExtractVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <typename T> RetType castAndCall(Ptr<VectorExtract> Inst) {
    return derived()(dyn_cast<T>(Inst));
  }

public:
  RetType visit(Ptr<VectorExtract> Inst) {
    using namespace vector_extract;
    using EKind = VectorExtractKind;
    switch (Inst->getVectorExtractKind()) {
    case EKind::SIMD128IntExtract: return castAndCall<SIMD128IntExtract>(Inst);
    case EKind::SIMD128FPExtract: return castAndCall<SIMD128FPExtract>(Inst);
    default: utility::unreachable();
    }
  }
};

template <typename Derived, typename RetType = void, bool Const = true>
class VectorInsertVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <typename T> RetType castAndCall(Ptr<VectorInsert> Inst) {
    return derived()(dyn_cast<T>(Inst));
  }

public:
  RetType visit(Ptr<VectorInsert> Inst) {
    using namespace vector_insert;
    using EKind = VectorInsertKind;
    switch (Inst->getVectorInsertKind()) {
    case EKind::SIMD128IntInsert: return castAndCall<SIMD128IntInsert>(Inst);
    case EKind::SIMD128FPInsert: return castAndCall<SIMD128FPInsert>(Inst);
    default: utility::unreachable();
    }
  }
};
} // namespace mir::instructions

#endif