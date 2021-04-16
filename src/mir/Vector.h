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
      VectorExtractKind Kind_, mir::Instruction *Operand_, unsigned LaneID_);
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
  void setLaneIndex(unsigned LaneID_);

  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

namespace vector_extract {
class SIMD128IntExtract : public VectorExtract {
  SIMD128IntLaneInfo LaneInfo;

public:
  SIMD128IntExtract(
      SIMD128IntLaneInfo LaneInfo, mir::Instruction *Operand_,
      unsigned LaneIndex_);
  SIMD128IntExtract(SIMD128IntExtract const &) = delete;
  SIMD128IntExtract(SIMD128IntExtract &&) noexcept = delete;
  SIMD128IntExtract &operator=(SIMD128IntExtract const &) = delete;
  SIMD128IntExtract &operator=(SIMD128IntExtract &&) noexcept = delete;
  ~SIMD128IntExtract() noexcept override;
};
} // namespace vector_extract

// class VectorInsert : public Instruction {};

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
} // namespace mir::instructions

#endif