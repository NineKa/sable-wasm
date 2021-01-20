#ifndef SABLE_INCLUDE_GUARD_BYTECODE_VALIDATION
#define SABLE_INCLUDE_GUARD_BYTECODE_VALIDATION

#include "Instruction.h"
#include "Module.h"
#include "Type.h"

#include <range/v3/core.hpp>
#include <range/v3/range/traits.hpp>
#include <range/v3/view/all.hpp>

#include <variant>
#include <vector>

namespace bytecode::validation {
class TypeVariable {
  std::size_t ID;

public:
  explicit TypeVariable(std::size_t ID_) : ID(ID_) {}
  std::size_t getID() const { return ID; }
  bool operator==(TypeVariable const &) const = default;
  auto operator<=>(TypeVariable const &) const = default;
};

class OperandStackElement {
  std::variant<ValueType, TypeVariable> Element;

public:
  /* Implicit */
  OperandStackElement(ValueType Type) : Element(Type) {}
  OperandStackElement(TypeVariable Variable) : Element(Variable) {}

  // clang-format off
  bool isValueType() const
  { return std::holds_alternative<ValueType>(Element); }
  bool isTypeVariable() const
  { return std::holds_alternative<TypeVariable>(Element); }

  ValueType &asValueType() { return std::get<ValueType>(Element); }
  TypeVariable &asTypeVariable() { return std::get<TypeVariable>(Element); }

  ValueType const &asValueType() const { return std::get<ValueType>(Element); }
  TypeVariable const &asTypeVariable() const
  { return std::get<TypeVariable>(Element); }

  template <typename Function> auto match(Function &&Fn)
  { return std::visit(std::forward<Function>(Fn), Element); }
  template <typename Function> auto match(Function &&Fn) const
  { return std::visit(std::forward<Function>(Fn), Element); }
  // clang-format on
};

struct OperandStack {
  template <ranges::input_range T, ranges::input_range U>
  bool operator()(T const &Ensures, U const &Promises) {
    using TT = ranges::range_value_t<T>;
    using UU = ranges::range_value_t<T>;
    static_assert(std::convertible_to<TT, OperandStackElement>);
    static_assert(std::convertible_to<UU, OperandStackElement>);
    Cursor = Stack.rbegin();
    for (auto const &Ensure : Ensures)
      if (!ensure(Ensure)) return false;
    if (Cursor == Stack.rend()) {
      Stack.clear();
    } else {
      auto Start = ++std::next(Cursor).base();
      auto End = Stack.end();
      Stack.erase(Start, End);
    }
    for (auto const &Promise : Promises) promise(Promise);
    return true;
  }

  void setEpsilon() { UnderEpsilon = true; }
  auto getRequirements() { return ranges::views::all(Requirements); }

private:
  std::vector<OperandStackElement> Stack;
  std::vector<OperandStackElement> Requirements;
  bool UnderEpsilon = false;
  decltype(Stack)::const_reverse_iterator Cursor;

  bool ensure(OperandStackElement const &Type);
  void promise(OperandStackElement Type);
};

class TypeError : std::runtime_error {
  std::vector<OperandStackElement> Expecting;
  std::vector<OperandStackElement> Actual;
  InstructionPtr *Site;

public:
  template <ranges::input_range T, ranges::input_range U>
  TypeError(InstructionPtr *Site_, T const &Expecting_, U const &Actual_)
      : std::runtime_error("type error"),
        Expecting(ranges::to<decltype(Expecting)>(Expecting_)),
        Actual(ranges::to<decltype(Actual)>(Actual_)), Site(Site_) {}

  auto expecting() const { return ranges::views::all(Expecting); }
  auto actual() const { return ranges::views::all(Actual); }
  InstructionPtr *site() const { return Site; }
};

} // namespace bytecode::validation

#endif
