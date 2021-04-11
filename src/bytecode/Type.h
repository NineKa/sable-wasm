#ifndef SABLE_INCLUDE_GUARD_BYTECODE_TYPE
#define SABLE_INCLUDE_GUARD_BYTECODE_TYPE

#include "../utility/Commons.h"

#include <fmt/format.h>
#include <range/v3/core.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/subrange.hpp>

#include <cassert>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <vector>

namespace bytecode {
enum class ValueTypeKind : unsigned char { I32, I64, F32, F64, V128 };

class ValueType {
  ValueTypeKind Kind;

public:
  constexpr ValueType(ValueTypeKind Kind_) : Kind(Kind_) {}
  ValueTypeKind getKind() const { return Kind; }
  bool isI32() const { return Kind == ValueTypeKind::I32; }
  bool isI64() const { return Kind == ValueTypeKind::I64; }
  bool isF32() const { return Kind == ValueTypeKind::F32; }
  bool isF64() const { return Kind == ValueTypeKind::F64; }
  bool isV128() const { return Kind == ValueTypeKind::V128; }

  bool operator==(ValueType const &Other) const = default;
};

namespace valuetypes {
inline constexpr ValueType I32(ValueTypeKind::I32);
inline constexpr ValueType I64(ValueTypeKind::I64);
inline constexpr ValueType F32(ValueTypeKind::F32);
inline constexpr ValueType F64(ValueTypeKind::F64);
inline constexpr ValueType V128(ValueTypeKind::V128);
} // namespace valuetypes

class FunctionType {
  std::vector<ValueType> ParamTypes;
  std::vector<ValueType> ResultTypes;

public:
  FunctionType(
      std::vector<ValueType> ParamTypes_, std::vector<ValueType> ResultTypes_)
      : ParamTypes(std::move(ParamTypes_)),
        ResultTypes(std::move(ResultTypes_)) {}

  template <ranges::input_range T, ranges::input_range U>
  FunctionType(T const &ParamTypes_, U const &ResultTypes_)
      : ParamTypes(ranges::to<std::vector<ValueType>>(ParamTypes_)),
        ResultTypes(ranges::to<std::vector<ValueType>>(ResultTypes_)) {}

  // clang-format off
  auto getParamTypes() const 
  { return ranges::make_subrange(ParamTypes.begin(), ParamTypes.end()); }
  auto getResultTypes() const 
  { return ranges::make_subrange(ResultTypes.begin(), ResultTypes.end()); }
  // clang-format on

  bool isVoidResult() const { return ResultTypes.empty(); }
  bool isSingleValueResult() const { return ResultTypes.size() == 1; }
  bool isMultiValueResult() const { return ResultTypes.size() > 1; }
  std::size_t getNumResult() const { return ResultTypes.size(); }
  std::size_t getNumParameter() const { return ParamTypes.size(); }

  bool operator==(FunctionType const &Other) const = default;
};

// clang-format off
template <typename T> concept limit_like_type = requires(T Type) {
  { Type.getMin() } -> std::convertible_to<std::uint32_t>;
  { Type.hasMax() } -> std::convertible_to<bool>;
  { Type.getMax() } -> std::convertible_to<std::uint32_t>;
};
// clang-format on

class MemoryType {
  std::uint32_t Min;
  std::optional<std::uint32_t> Max;

public:
  explicit MemoryType(std::uint32_t Min_) : Min(Min_), Max(std::nullopt) {}
  MemoryType(std::uint32_t Min_, std::uint32_t Max_) : Min(Min_), Max(Max_) {
    assert(Min <= Max && "memory type constraint");
  }
  std::uint32_t getMin() const { return Min; }
  bool hasMax() const { return Max.has_value(); }
  std::uint32_t getMax() const {
    assert(hasMax() && "memory type maximum is not set");
    return *Max;
  }
  bool operator==(MemoryType const &Other) const = default;
};

class TableType {
  std::uint32_t Min;
  std::optional<std::uint32_t> Max;

public:
  explicit TableType(std::uint32_t Min_) : Min(Min_), Max(std::nullopt) {}
  TableType(std::uint32_t Min_, std::uint32_t Max_) : Min(Min_), Max(Max_) {
    assert(Min <= Max && "table type constraint");
  }
  std::uint32_t getMin() const { return Min; }
  bool hasMax() const { return Max.has_value(); }
  std::uint32_t getMax() const {
    assert(hasMax() && "table type maximum is not set");
    return *Max;
  }
  bool operator==(TableType const &Other) const = default;
};

enum class MutabilityKind : unsigned char { Const, Var };
class GlobalType {
  MutabilityKind Mut;
  ValueType Type;

public:
  GlobalType(MutabilityKind Mut_, ValueType Type_) : Mut(Mut_), Type(Type_) {}
  bool isConst() const { return Mut == MutabilityKind::Const; }
  bool isVar() const { return Mut == MutabilityKind::Var; }
  ValueType getType() const { return Type; }
  MutabilityKind getMutability() const { return Mut; }
  bool operator==(GlobalType const &Other) const = default;
};
} // namespace bytecode

////////////////////////////////// Formatters //////////////////////////////////

template <> struct fmt::formatter<bytecode::ValueType, char> {
  template <typename Context> auto parse(Context &&CTX) { return CTX.begin(); }
  template <typename Context>
  auto format(bytecode::ValueType const &Type, Context &&CTX) {
    using Kind = bytecode::ValueTypeKind;
    switch (Type.getKind()) {
    case Kind::I32: return format_to(CTX.out(), "i32");
    case Kind::I64: return format_to(CTX.out(), "i64");
    case Kind::F32: return format_to(CTX.out(), "f32");
    case Kind::F64: return format_to(CTX.out(), "f64");
    case Kind::V128: return format_to(CTX.out(), "v128");
    default: utility::unreachable();
    }
  }
};

template <> struct fmt::formatter<bytecode::FunctionType, char> {
  template <typename T> constexpr auto parse(T &&CTX) { return CTX.begin(); }
  template <typename Context>
  auto format(bytecode::FunctionType const &Type, Context &&CTX) const {
    auto Out = CTX.out();
    char const *Separator = "";
    *Out++ = '[';
    for (auto const &ParamType : Type.getParamTypes()) {
      Out = format_to(Out, "{}{}", Separator, ParamType);
      Separator = ", ";
    }
    Separator = "";
    Out = format_to(Out, "] -> [");
    for (auto const &ResultType : Type.getResultTypes()) {
      Out = format_to(Out, "{}{}", Separator, ResultType);
      Separator = ", ";
    }
    *Out++ = ']';
    return Out;
  }
};

namespace bytecode::detail {
struct LimitLikeTypeFormatterBase {
  template <typename Iterator, limit_like_type T>
  Iterator formatLimit(Iterator Out, T const &Type) const {
    if (!Type.hasMax()) return fmt::format_to(Out, "{{min {}}}", Type.getMin());
    auto Min = Type.getMin();
    auto Max = Type.getMax();
    return fmt::format_to(Out, "{{min {}, max {}}}", Min, Max);
  }
};
} // namespace bytecode::detail

template <>
struct fmt::formatter<bytecode::MemoryType, char> :
    bytecode::detail::LimitLikeTypeFormatterBase {
  template <typename T> constexpr auto parse(T &&CTX) { return CTX.begin(); }
  template <typename Context>
  auto format(bytecode::MemoryType const &Type, Context &&CTX) const {
    return formatLimit(CTX.out(), Type);
  }
};

template <>
struct fmt::formatter<bytecode::TableType, char> :
    bytecode::detail::LimitLikeTypeFormatterBase {
  template <typename T> constexpr auto parse(T &&CTX) { return CTX.begin(); }
  template <typename Context>
  auto format(bytecode::TableType const &Type, Context &&CTX) const {
    return format_to(formatLimit(CTX.out(), Type), " funcref");
  }
};

template <> struct fmt::formatter<bytecode::GlobalType, char> {
  template <typename T> constexpr auto parse(T &&CTX) { return CTX.begin(); }
  template <typename Context>
  auto format(bytecode::GlobalType const &Type, Context &&CTX) const {
    using Kind = bytecode::MutabilityKind;
    switch (Type.getMutability()) {
    case Kind::Const: return format_to(CTX.out(), "const {}", Type.getType());
    case Kind::Var: return format_to(CTX.out(), "var {}", Type.getType());
    default: utility::unreachable();
    }
  }
};

#endif
