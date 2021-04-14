#ifndef SABLE_INCLUDE_GUARD_MIR_UNARY_OP
#define SABLE_INCLUDE_GUARD_MIR_UNARY_OP

#include "Instruction.h"

#include <fmt/format.h>

namespace mir::instructions {

enum class UnaryKind {
  IntUnary,
  FPUnary,
  SIMD128Unary,
  SIMD128IntUnary,
  SIMD128FPUnary
};

class Unary : public Instruction {
  UnaryKind Kind;
  Instruction *Operand;

public:
  Unary(UnaryKind Kind_, Instruction *Operand_);
  Unary(Unary const &) = delete;
  Unary(Unary &&) noexcept = delete;
  Unary &operator=(Unary const &) = delete;
  Unary &operator=(Unary &&) noexcept = delete;
  ~Unary() noexcept override;

  UnaryKind getUnaryKind() const;
  bool isIntUnary() const;
  bool isFPUnary() const;
  bool isSIMD128Unary() const;
  bool isSIMD128IntUnary() const;
  bool isSIMD128FPUnary() const;

  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

SABLE_DEFINE_IS_A(Unary)
SABLE_DEFINE_DYN_CAST(Unary)

namespace unary {
enum class IntUnaryOperator : std::uint8_t { Eqz, Clz, Ctz, Popcnt };
class IntUnary : public Unary {
  IntUnaryOperator Operator;

public:
  IntUnary(IntUnaryOperator Operator_, Instruction *Operand_);
  IntUnary(IntUnary const &) = delete;
  IntUnary(IntUnary &&) noexcept = delete;
  IntUnary &operator=(IntUnary const &) = delete;
  IntUnary &operator=(IntUnary &&) noexcept = delete;
  ~IntUnary() noexcept override;

  IntUnaryOperator getOperator() const;
  void setOperator(IntUnaryOperator Operator_);

  static bool classof(Unary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

// clang-format off
enum class FPUnaryOperator : std::uint8_t
{ Abs, Neg, Ceil, Floor, Trunc, Nearest, Sqrt };
// clang-format on
class FPUnary : public Unary {
  FPUnaryOperator Operator;

public:
  FPUnary(FPUnaryOperator Operator_, Instruction *Operand_);
  FPUnary(FPUnary const &) = delete;
  FPUnary(FPUnary &&) noexcept = delete;
  FPUnary &operator=(FPUnary const &) = delete;
  FPUnary &operator=(FPUnary &&) noexcept = delete;
  ~FPUnary() noexcept override;

  FPUnaryOperator getOperator() const;
  void setOperator(FPUnaryOperator Operator_);

  static bool classof(Unary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

enum class SIMD128UnaryOperator : std::uint8_t { Not, AnyTrue };
class SIMD128Unary : public Unary {
  SIMD128UnaryOperator Operator;

public:
  SIMD128Unary(SIMD128UnaryOperator Operator_, Instruction *Operand_);
  SIMD128Unary(SIMD128Unary const &) = delete;
  SIMD128Unary(SIMD128Unary &&) noexcept = delete;
  SIMD128Unary &operator=(SIMD128Unary const &) = delete;
  SIMD128Unary &operator=(SIMD128Unary &&) noexcept = delete;
  ~SIMD128Unary() noexcept override;

  SIMD128UnaryOperator getOperator() const;
  void setOperator(SIMD128UnaryOperator Operator_);

  static bool classof(Unary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

// clang-format off
enum class SIMD128IntUnaryOperator : std::uint8_t
{ Neg, Abs, AllTrue, Bitmask };
// clang-format on
class SIMD128IntUnary : public Unary {
  SIMD128IntUnaryOperator Operator;
  SIMD128IntLaneInfo LaneInfo;

public:
  SIMD128IntUnary(
      SIMD128IntUnaryOperator Operator_, SIMD128IntLaneInfo LaneInfo_,
      Instruction *Operand_);
  SIMD128IntUnary(SIMD128IntUnary const &) = delete;
  SIMD128IntUnary(SIMD128IntUnary &&) noexcept = delete;
  SIMD128IntUnary &operator=(SIMD128IntUnary const &) = delete;
  SIMD128IntUnary &operator=(SIMD128IntUnary &&) noexcept = delete;
  ~SIMD128IntUnary() noexcept override;

  SIMD128IntUnaryOperator getOperator() const;
  void setOperator(SIMD128IntUnaryOperator Operator_);
  SIMD128IntLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128IntLaneInfo LaneInfo_);

  static bool classof(Unary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

// clang-format off
enum class SIMD128FPUnaryOperator : std::uint8_t
{ Neg, Abs, Sqrt, Ceil, Floor, Trunc, Nearest };
// clang-format on
class SIMD128FPUnary : public Unary {
  SIMD128FPUnaryOperator Operator;
  SIMD128FPLaneInfo LaneInfo;

public:
  SIMD128FPUnary(
      SIMD128FPUnaryOperator Operator_, SIMD128FPLaneInfo LnaeInfo_,
      Instruction *Operand_);
  SIMD128FPUnary(SIMD128FPUnary const &) = delete;
  SIMD128FPUnary(SIMD128FPUnary &&) noexcept = delete;
  SIMD128FPUnary &operator=(SIMD128FPUnary const &) = delete;
  SIMD128FPUnary &operator=(SIMD128FPUnary &&) noexcept = delete;
  ~SIMD128FPUnary() noexcept override;

  SIMD128FPUnaryOperator getOperator() const;
  void setOperator(SIMD128FPUnaryOperator Operator_);
  SIMD128FPLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128FPLaneInfo LaneInfo_);

  static bool classof(Unary const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};
} // namespace unary

template <typename Derived, typename RetType = void, bool Const = true>
class UnaryVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <typename T> RetType castAndCall(Ptr<Unary> Inst) {
    return derived()(dyn_cast<T>(Inst));
  }

public:
  RetType visit(Ptr<Unary> Inst) {
    using namespace unary;
    using UKind = UnaryKind;
    // clang-format off
    switch (Inst->getUnaryKind()) {
    case UKind::IntUnary       : return castAndCall<IntUnary>(Inst);
    case UKind::FPUnary        : return castAndCall<FPUnary>(Inst);
    case UKind::SIMD128Unary   : return castAndCall<SIMD128Unary>(Inst);
    case UKind::SIMD128IntUnary: return castAndCall<SIMD128IntUnary>(Inst);
    case UKind::SIMD128FPUnary : return castAndCall<SIMD128FPUnary>(Inst);
    default: utility::unreachable();
    }
    // clang-format on
  }
};
} // namespace mir::instructions

namespace fmt {
template <> struct formatter<mir::instructions::unary::IntUnaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using UnaryOperator = mir::instructions::unary::IntUnaryOperator;
  char const *toString(UnaryOperator const &Operator);
  template <typename CTX> auto format(UnaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <> struct formatter<mir::instructions::unary::FPUnaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using UnaryOperator = mir::instructions::unary::FPUnaryOperator;
  char const *toString(UnaryOperator const &Operator);
  template <typename CTX> auto format(UnaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <> struct formatter<mir::instructions::unary::SIMD128UnaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using UnaryOperator = mir::instructions::unary::SIMD128UnaryOperator;
  char const *toString(UnaryOperator const &Operator);
  template <typename CTX> auto format(UnaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <>
struct formatter<mir::instructions::unary::SIMD128IntUnaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using UnaryOperator = mir::instructions::unary::SIMD128IntUnaryOperator;
  char const *toString(UnaryOperator const &Operator);
  template <typename CTX> auto format(UnaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <> struct formatter<mir::instructions::unary::SIMD128FPUnaryOperator> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  using UnaryOperator = mir::instructions::unary::SIMD128FPUnaryOperator;
  char const *toString(UnaryOperator const &Operator);
  template <typename CTX> auto format(UnaryOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};
} // namespace fmt

#endif