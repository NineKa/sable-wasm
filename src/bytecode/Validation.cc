#include "Validation.h"

#include <boost/preprocessor.hpp>

#include <memory>

using namespace bytecode::valuetypes;
using namespace bytecode::instructions;

namespace bytecode::validation {
bool OperandStack::ensure(OperandStackElement const &Type) {
  if (Cursor != Stack.rend()) {
    auto const TypeVariableFn = [&](TypeVariable const &Variable) {
      for (auto &StackType : Stack)
        if (StackType.isTypeVariable() &&
            (StackType.asTypeVariable() == Variable))
          StackType = Type;
      for (auto &Requirement : Requirements)
        if (Requirement.isTypeVariable() &&
            (Requirement.asTypeVariable() == Variable))
          Requirement = Type;
      Cursor = std::next(Cursor);
      return true;
    };
    auto const ValueTypeFn = [&](bytecode::ValueType const &ValueType) {
      if (Type.isTypeVariable()) return false;
      if (Type.asValueType() != ValueType) return false;
      Cursor = std::next(Cursor);
      return true;
    };
    return (*Cursor).match(utility::Overload{TypeVariableFn, ValueTypeFn});
  }
  assert(Cursor == Stack.rend());
  if (UnderEpsilon) {
    Requirements.push_back(Type);
    return true;
  }
  return false;
}

void OperandStack::promise(OperandStackElement Type) { Stack.push_back(Type); }

namespace {
class LabelStack {
  using FunctionTypeResultsView =
      decltype(std::declval<FunctionType>().getResultTypes());
  using value_type =
      utility::Sum<FunctionTypeResultsView, ValueType, BlockResultUnit>;
  std::vector<value_type> Stack;

public:
  value_type const &operator[](LabelIDX const &Index) const {
    auto CastedIndex = static_cast<std::size_t>(Index);
    assert(CastedIndex < Stack.size());
    auto Iterator = std::next(ranges::rbegin(Stack), CastedIndex);
    return *Iterator;
  }
  void pop() { Stack.pop_back(); }

  void push() { Stack.emplace_back(BlockResultUnit{}); }
  void push(ValueType const &Type) { Stack.emplace_back(Type); }
  void push(FunctionType const &Type) {
    Stack.emplace_back(Type.getResultTypes());
  }
};

class TypeChecker : InstVisitorBase<TypeChecker, bool> {
  ModuleView Module;
  LabelStack Labels;
  OperandStack Stack;

public:
#define ON(Name, ParamTypes, ResultTypes)                                      \
  bool operator()(Name const &) {                                              \
    auto Parameters = {                                                        \
        BOOST_PP_LIST_ENUM(BOOST_PP_ARRAY_TO_LIST(ParamTypes))};               \
    auto Results = {BOOST_PP_LIST_ENUM(BOOST_PP_ARRAY_TO_LIST(ResultTypes))};  \
    return Stack(Parameters, Results);                                         \
  }
  // clang-format off
  ON(I32Eqz, (1, (I32)     ), (1, (I32))) // [i32]      -> [i32]
  ON(I32Eq , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32Ne , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32LtS, (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32LtU, (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32GtS, (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32GtU, (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32LeS, (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32LeU, (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32GeS, (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32GeU, (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  // clang-format on
#undef ON
};
} // namespace
} // namespace bytecode::validation
