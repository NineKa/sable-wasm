#ifndef SABLE_INCLUDE_GUARD_MIR_BINARY
#define SABLE_INCLUDE_GUARD_MIR_BINARY

#include "Instruction.h"

#include <fmt/format.h>

namespace mir::instructions {

enum class BinaryKind {
  IntBinary,
  FPBinary,
  SIMD128Binary,
  SIMD128IntBinary,
  SIMD128FPBinary
};

class Binary : public Instruction {
  BinaryKind Kind;
  Instruction *LHS;
  Instruction *RHS;

public:
  Binary(BinaryKind Kind_, Instruction *LHS_, Instruction *RHS_);
  Binary(Binary const &) = delete;
  Binary(Binary &&) noexcept = delete;
  Binary &operator=(Binary const &) = delete;
  Binary &operator=(Binary &&) noexcept = delete;
  ~Binary() noexcept override;

  BinaryKind getBinaryKind() const;
  bool isIntBinary() const;
  bool isFPBinary() const;
  bool isSIMD128Binary() const;
  bool isSIMD128IntBinary() const;
  bool isSIMD128FPBinary() const;

  Instruction *getLHS() const;
  void setLHS(Instruction *LHS_);
  Instruction *getRHS() const;
  void setRHS(Instruction *RHS_);

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

SABLE_DEFINE_IS_A(Binary)
SABLE_DEFINE_DYN_CAST(Binary)

namespace binary {
// clang-format off
enum class IntBinaryOperator : std::uint8_t {
  Add, Sub, Mul, DivS, DivU, RemS, RemU, And, Or, Xor,
  Shl, ShrS, ShrU, Rotl, Rotr
};
// clang-format on
class IntBinary : public Binary {
  IntBinaryOperator Operator;

public:
  IntBinary(IntBinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_);
  IntBinary(IntBinary const &) = delete;
  IntBinary(IntBinary &&) noexcept = delete;
  IntBinary &operator=(IntBinary const &) = delete;
  IntBinary &operator=(IntBinary &&) noexcept = delete;
  ~IntBinary() noexcept override;

  IntBinaryOperator getOperator() const;
  void setOperator(IntBinaryOperator Operator_);

  static bool classof(Binary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

// clang-format off
enum class FPBinaryOperator : std::uint8_t
{ Add, Sub, Mul, Div, Min, Max, CopySign };
// clang-format on

class FPBinary : public Binary {
  FPBinaryOperator Operator;

public:
  FPBinary(FPBinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_);
  FPBinary(FPBinary const &) = delete;
  FPBinary(FPBinary &&) noexcept = delete;
  FPBinary &operator=(FPBinary const &) = delete;
  FPBinary &operator=(FPBinary &&) noexcept = delete;
  ~FPBinary() noexcept override;

  FPBinaryOperator getOperator() const;
  void setOperator(FPBinaryOperator Operator_);

  static bool classof(Binary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

enum class SIMD128BinaryOperator : std::uint8_t { And, Or, Xor, AndNot };
class SIMD128Binary : public Binary {
  SIMD128BinaryOperator Operator;

public:
  SIMD128Binary(
      SIMD128BinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_);
  SIMD128Binary(SIMD128Binary const &) = delete;
  SIMD128Binary(SIMD128Binary &&) noexcept = delete;
  SIMD128Binary &operator=(SIMD128Binary const &) = delete;
  SIMD128Binary &operator=(SIMD128Binary &&) noexcept = delete;
  ~SIMD128Binary() noexcept override;

  SIMD128BinaryOperator getOperator() const;
  void setOperator(SIMD128BinaryOperator Operator_);

  static bool classof(Binary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

// clang-format off
enum class SIMD128IntBinaryOperator : std::uint8_t 
{ Add, Sub, Mul, 
  ExtMulLowS, ExtMulLowU, ExtMulHighS, ExtMulHighU, 
  ExtAddPairwise,
  AddSat, SubSat, Min, Max, AvgrU };
// clang-format on
class SIMD128IntBinary : public Binary {
  SIMD128IntBinaryOperator Operator;
  SIMD128IntLaneInfo LaneInfo;

public:
  SIMD128IntBinary(
      SIMD128IntBinaryOperator Operator_, SIMD128IntLaneInfo LaneInfo_,
      Instruction *LHS_, Instruction *RHS_);
  SIMD128IntBinary(SIMD128IntBinary const &) = delete;
  SIMD128IntBinary(SIMD128IntBinary &&) noexcept = delete;
  SIMD128IntBinary &operator=(SIMD128IntBinary const &) = delete;
  SIMD128IntBinary &operator=(SIMD128IntBinary &&) noexcept = delete;
  ~SIMD128IntBinary() noexcept override;

  SIMD128IntBinaryOperator getOperator() const;
  void setOperator(SIMD128IntBinaryOperator Operator_);
  SIMD128IntLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128IntLaneInfo LaneInfo_);

  static bool classof(Binary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

// clang-format off
enum class SIMD128FPBinaryOperator : std::uint8_t 
{ Add, Sub, Div, Mul, Min, Max, PMin, PMax };
// clang-format on
class SIMD128FPBinary : public Binary {
  SIMD128FPBinaryOperator Operator;
  SIMD128FPLaneInfo LaneInfo;

public:
  SIMD128FPBinary(
      SIMD128FPBinaryOperator Operator_, SIMD128FPLaneInfo LaneInfo_,
      Instruction *LHS_, Instruction *RHS_);
  SIMD128FPBinary(SIMD128FPBinary const &) = delete;
  SIMD128FPBinary(SIMD128FPBinary &&) noexcept = delete;
  SIMD128FPBinary &operator=(SIMD128FPBinary const &) = delete;
  SIMD128FPBinary &operator=(SIMD128FPBinary &&) noexcept = delete;
  ~SIMD128FPBinary() noexcept override;

  SIMD128FPBinaryOperator getOperator() const;
  void setOperator(SIMD128FPBinaryOperator Operator_);
  SIMD128FPLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128FPLaneInfo LaneInfo_);

  static bool classof(Binary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};
} // namespace binary

template <typename Derived, typename RetType = void, bool Const = true>
class BinaryVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <typename T> RetType castAndCall(Ptr<Binary> Inst) {
    return derived()(dyn_cast<T>(Inst));
  }

public:
  RetType visit(Ptr<Binary> Inst) {
    using namespace binary;
    // clang-format off
    switch (Inst->getBinaryKind()) {
    case BinaryKind::IntBinary       : return castAndCall<IntBinary>(Inst);
    case BinaryKind::FPBinary        : return castAndCall<FPBinary>(Inst);
    case BinaryKind::SIMD128Binary   : return castAndCall<SIMD128Binary>(Inst);
    case BinaryKind::SIMD128IntBinary: 
      return castAndCall<SIMD128IntBinary>(Inst);
    case BinaryKind::SIMD128FPBinary : 
      return castAndCall<SIMD128FPBinary>(Inst);
    default: utility::unreachable();
    }
    // clang-format on
  }
};
} // namespace mir::instructions

namespace fmt {
template <> struct formatter<mir::instructions::binary::IntBinaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using BinaryOperator = mir::instructions::binary::IntBinaryOperator;
  char const *toString(BinaryOperator const &Operator);
  template <typename CTX> auto format(BinaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <> struct formatter<mir::instructions::binary::FPBinaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using BinaryOperator = mir::instructions::binary::FPBinaryOperator;
  char const *toString(BinaryOperator const &Operator);
  template <typename CTX> auto format(BinaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <> struct formatter<mir::instructions::binary::SIMD128BinaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using BinaryOperator = mir::instructions::binary::SIMD128BinaryOperator;
  char const *toString(BinaryOperator const &Operator);
  template <typename CTX> auto format(BinaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <>
struct formatter<mir::instructions::binary::SIMD128IntBinaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using BinaryOperator = mir::instructions::binary::SIMD128IntBinaryOperator;
  char const *toString(BinaryOperator const &Operator);
  template <typename CTX> auto format(BinaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <>
struct formatter<mir::instructions::binary::SIMD128FPBinaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using BinaryOperator = mir::instructions::binary::SIMD128FPBinaryOperator;
  char const *toString(BinaryOperator const &Operator);
  template <typename CTX> auto format(BinaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};
} // namespace fmt

#endif