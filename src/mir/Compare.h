#ifndef SABLE_INCLUDE_GUARD_MIR_COMPARE
#define SABLE_INCLUDE_GUARD_MIR_COMPARE

#include "Instruction.h"

#include <fmt/format.h>

namespace mir::instructions {
enum class CompareKind {
  IntCompare,
  FPCompare,
  SIMD128IntCompare,
  SIMD128FPCompare
};

class Compare : public Instruction {
  CompareKind Kind;
  Instruction *LHS;
  Instruction *RHS;

public:
  Compare(CompareKind Kind_, Instruction *LHS_, Instruction *RHS_);
  Compare(Compare const &) = delete;
  Compare(Compare &&) noexcept = delete;
  Compare &operator=(Compare const &) = delete;
  Compare &operator=(Compare &&) noexcept = delete;
  ~Compare() noexcept override;

  CompareKind getCompareKind() const;
  bool isIntCompare() const;
  bool isFPCompare() const;
  bool isSIMD128IntCompare() const;
  bool isSIMD128FPCompare() const;

  Instruction *getLHS() const;
  void setLHS(Instruction *LHS_);
  Instruction *getRHS() const;
  void setRHS(Instruction *RHS_);

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

SABLE_DEFINE_IS_A(Compare)
SABLE_DEFINE_DYN_CAST(Compare)

namespace compare {
// clang-format off
enum class IntCompareOperator : std::uint8_t 
{ Eq, Ne, LtS, LtU, GtS, GtU, LeS, LeU, GeS, GeU };
// clang-format on
class IntCompare : public Compare {
  IntCompareOperator Operator;

public:
  IntCompare(
      IntCompareOperator Operator_, Instruction *LHS_, Instruction *RHS_);
  IntCompare(IntCompare const &) = delete;
  IntCompare(IntCompare &&) noexcept = delete;
  IntCompare &operator=(IntCompare const &) = delete;
  IntCompare &operator=(IntCompare &&) noexcept = delete;
  ~IntCompare() noexcept override;

  IntCompareOperator getOperator() const;
  void setOperator(IntCompareOperator Operator_);

  static bool classof(Compare const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

enum class FPCompareOperator : std::uint8_t { Eq, Ne, Lt, Gt, Le, Ge };
class FPCompare : public Compare {
  FPCompareOperator Operator;

public:
  FPCompare(FPCompareOperator Operator_, Instruction *LHS_, Instruction *RHS_);
  FPCompare(FPCompare const &) = delete;
  FPCompare(FPCompare &&) noexcept = delete;
  FPCompare &operator=(FPCompare const &) = delete;
  FPCompare &operator=(FPCompare &&) noexcept = delete;
  ~FPCompare() noexcept override;

  FPCompareOperator getOperator() const;
  void setOperator(FPCompareOperator Operator_);

  static bool classof(Compare const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

// clang-format off
enum class SIMD128IntCompareOperator : std::uint8_t
{ Eq, Ne, LtS, LtU, GtS, GtU, LeS, LeU, GeS, GeU };
// clang-format on
class SIMD128IntCompare : public Compare {
  SIMD128IntCompareOperator Operator;
  SIMD128IntLaneInfo LaneInfo;

public:
  SIMD128IntCompare(
      SIMD128IntCompareOperator Operator_, SIMD128IntLaneInfo LaneInfo_,
      Instruction *LHS_, Instruction *RHS_);
  SIMD128IntCompare(SIMD128IntCompare const &) = delete;
  SIMD128IntCompare(SIMD128IntCompare &&) noexcept = delete;
  SIMD128IntCompare &operator=(SIMD128IntCompare const &) = delete;
  SIMD128IntCompare &operator=(SIMD128IntCompare &&) noexcept = delete;
  ~SIMD128IntCompare() noexcept override;

  SIMD128IntCompareOperator getOperator() const;
  void setOperator(SIMD128IntCompareOperator Operator_);
  SIMD128IntLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128IntLaneInfo LaneInfo_);

  static bool classof(Compare const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

enum class SIMD128FPCompareOperator : std::uint8_t { Eq, Ne, Lt, Gt, Le, Ge };
class SIMD128FPCompare : public Compare {
  SIMD128FPCompareOperator Operator;
  SIMD128FPLaneInfo LaneInfo;

public:
  SIMD128FPCompare(
      SIMD128FPCompareOperator Operator_, SIMD128FPLaneInfo LaneInfo_,
      Instruction *LHS_, Instruction *RHS_);
  SIMD128FPCompare(SIMD128FPCompare const &) = delete;
  SIMD128FPCompare(SIMD128FPCompare &&) noexcept = delete;
  SIMD128FPCompare &operator=(SIMD128FPCompare const &) = delete;
  SIMD128FPCompare &operator=(SIMD128FPCompare &&) noexcept = delete;
  ~SIMD128FPCompare() noexcept override;

  SIMD128FPCompareOperator getOperator() const;
  void setOperator(SIMD128FPCompareOperator Operator_);
  SIMD128FPLaneInfo getLaneInfo() const;
  void setLaneInfo(SIMD128FPLaneInfo LaneInfo_);

  static bool classof(Compare const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

} // namespace compare

template <typename Derived, typename RetType = void, bool Const = true>
class CompareVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <typename T> RetType castAndCall(Ptr<Compare> Inst) {
    return derived()(dyn_cast<T>(Inst));
  }

public:
  RetType visit(Ptr<Compare> Inst) {
    using namespace compare;
    using CKind = CompareKind;
    // clang-format off
    switch (Inst->getCompareKind()) {
    case CKind::IntCompare       : return castAndCall<IntCompare>(Inst);
    case CKind::FPCompare        : return castAndCall<FPCompare>(Inst);
    case CKind::SIMD128IntCompare: return castAndCall<SIMD128IntCompare>(Inst);
    case CKind::SIMD128FPCompare : return castAndCall<SIMD128FPCompare>(Inst);
    default: utility::unreachable();
    }
    // clang-format on
  }
};
} // namespace mir::instructions

namespace fmt {
template <> struct formatter<mir::instructions::compare::IntCompareOperator> {
  using CompareOperator = mir::instructions::compare::IntCompareOperator;
  char const *toString(CompareOperator const &Operator);
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  template <typename CTX>
  auto format(CompareOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <> struct formatter<mir::instructions::compare::FPCompareOperator> {
  using CompareOperator = mir::instructions::compare::FPCompareOperator;
  char const *toString(CompareOperator const &Operator);
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  template <typename CTX>
  auto format(CompareOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <>
struct formatter<mir::instructions::compare::SIMD128IntCompareOperator> {
  using CompareOperator = mir::instructions::compare::SIMD128IntCompareOperator;
  char const *toString(CompareOperator const &Operator);
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  template <typename CTX>
  auto format(CompareOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};

template <>
struct formatter<mir::instructions::compare::SIMD128FPCompareOperator> {
  using CompareOperator = mir::instructions::compare::SIMD128FPCompareOperator;
  char const *toString(CompareOperator const &Operator);
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  template <typename CTX>
  auto format(CompareOperator const &Operator, CTX &&C) {
    return fmt::format_to(C.out(), toString(Operator));
  }
};
} // namespace fmt

#endif