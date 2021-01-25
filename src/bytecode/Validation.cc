#include "Validation.h"

#include <boost/preprocessor.hpp>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/iterator.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/single.hpp>

#include <array>
#include <bit>
#include <optional>
#include <span>
#include <type_traits>

using namespace bytecode::valuetypes;
using namespace bytecode::instructions;

namespace bytecode::validation {
using ErrorPtr = std::unique_ptr<ValidationError>;

/////////////////////////////// Type Validation ////////////////////////////////
bool validate(ValueType const &Type) {
  switch (Type.getKind()) {
  case ValueTypeKind::I32:
  case ValueTypeKind::I64:
  case ValueTypeKind::F32:
  case ValueTypeKind::F64: return true;
  default: return false;
  }
}

bool validate(FunctionType const &Type) {
  for (auto const &ParamType : Type.getParamTypes())
    if (!validate(ParamType)) return false;
  for (auto const &ResultType : Type.getResultTypes())
    if (!validate(ResultType)) return false;
  return true;
}

bool validate(GlobalType const &Type) {
  switch (Type.getMutability()) {
  case MutabilityKind::Const:
  case MutabilityKind::Var: break;
  default: return false;
  }
  return validate(Type.getType());
}

namespace {
namespace detail {
template <limit_like_type T>
bool validateLimitLikeType(T const &Type, std::uint64_t K) {
  if (Type.getMin() > K) return false;
  if (Type.hasMax()) {
    if (Type.getMax() > K) return false;
    if (Type.getMax() < Type.getMin()) return false;
  }
  return true;
}
} // namespace detail
} // namespace

bool validate(MemoryType const &Type) {
  return detail::validateLimitLikeType(Type, std::uint64_t(1) << 32);
}
bool validate(TableType const &Type) {
  return detail::validateLimitLikeType(Type, std::uint64_t(1) << 16);
}

///////////////////////////////// OperandStack /////////////////////////////////
namespace {
class OperandStack {
  std::vector<OperandStackElement> Stack;
  std::vector<OperandStackElement> Requirements;
  bool UnderEpsilon = false;
  decltype(Stack)::const_reverse_iterator Cursor;

  bool ensure(OperandStackElement const &Type);
  void promise(OperandStackElement Type) { Stack.push_back(Type); }

public:
  template <ranges::input_range T, ranges::input_range U>
  bool operator()(T const &Ensures, U const &Promises) {
    using RangeValueT = ranges::range_value_t<T>;
    using RangeValueU = ranges::range_value_t<T>;
    static_assert(std::convertible_to<RangeValueT, OperandStackElement>);
    static_assert(std::convertible_to<RangeValueU, OperandStackElement>);
    Cursor = Stack.rbegin();
    for (auto const &Ensure : ranges::views::reverse(Ensures))
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
  bool getEpsilon() const { return UnderEpsilon; }
  auto getRequirements() { return ranges::views::all(Requirements); }
  void clear() {
    Stack.clear();
    Requirements.clear();
    UnderEpsilon = true;
  }

  bool empty() const { return Stack.empty(); }
  std::size_t size() const { return Stack.size(); }

  std::span<OperandStackElement const> recover(std::size_t NumOperandTypes) {
    if (Stack.size() <= NumOperandTypes) return Stack;
    std::span<OperandStackElement const> View(Stack);
    return View.subspan(Stack.size() - NumOperandTypes, NumOperandTypes);
  }

  std::optional<OperandStackElement> peek(std::size_t Offset = 0) const {
    if (!(Offset < Stack.size())) {
      if (!UnderEpsilon) return std::nullopt;
      return TypeVariable(Requirements.size());
    }
    auto Iterator = std::next(ranges::rbegin(Stack), Offset);
    return *Iterator;
  }
};

bool OperandStack::ensure(OperandStackElement const &Type) {
  if (Cursor != Stack.rend()) {
    auto const TypeVariableFn = [&](TypeVariable const &Variable) {
      for (auto &StackType : Stack)
        if (std::holds_alternative<TypeVariable>(StackType) &&
            (std::get<TypeVariable>(StackType) == Variable))
          StackType = Type;
      for (auto &Requirement : Requirements)
        if (std::holds_alternative<TypeVariable>(Requirement) &&
            (std::get<TypeVariable>(Requirement) == Variable))
          Requirement = Type;
      Cursor = std::next(Cursor);
      return true;
    };
    auto const ValueTypeFn = [&](bytecode::ValueType const &ValueType_) {
      if (std::holds_alternative<TypeVariable>(Type)) return false;
      if (std::get<ValueType>(Type) != ValueType_) return false;
      Cursor = std::next(Cursor);
      return true;
    };
    return std::visit(utility::Overload{TypeVariableFn, ValueTypeFn}, *Cursor);
  }
  assert(Cursor == Stack.rend());
  if (UnderEpsilon) {
    Requirements.push_back(Type);
    return true;
  }
  return false;
}
} // namespace

////////////////////////////////// LabelStack //////////////////////////////////
namespace {
class LabelStack {
  std::vector<ValueType> Storage;
  std::vector<std::size_t> Stack;

public:
  std::optional<std::span<ValueType const>>
  operator[](LabelIDX const &Index) const {
    auto CastedIndex = static_cast<std::size_t>(Index);
    if (!(CastedIndex < Stack.size())) return std::nullopt;
    std::span<ValueType const> View(Storage);
    if (CastedIndex == 0) {
      auto StartPos = Stack.back();
      auto EndPos = Storage.size();
      return View.subspan(StartPos, EndPos - StartPos);
    }
    auto StartPos = *std::next(Stack.rbegin(), CastedIndex);
    auto EndPos = *std::next(Stack.rbegin(), CastedIndex - 1);
    return View.subspan(StartPos, EndPos - StartPos);
  }

  void pop() { Stack.pop_back(); }

  template <ranges::input_range T> void push(T const &LabelExpects) {
    Stack.push_back(Storage.size());
    ranges::copy(LabelExpects, ranges::back_inserter(Storage));
  }

  bool empty() const { return Stack.empty(); }
};
} // namespace

///////////////////////////// ExprValidationContext ////////////////////////////
namespace {
class ExprValidationContext {
  ModuleView Module;
  std::optional<entities::Function const *> EnclosingFunction;
  LabelStack Labels;

public:
  template <typename T> struct Wrapper {
    ExprValidationContext const &Context;
    decltype(auto) operator[](T const &Index) {
      return Context.Module.get(Index);
    }
  };

  struct LocalWrapper {
    ExprValidationContext const &Context;
    std::optional<ValueType> operator[](LocalIDX const &Index) {
      if (!Context.EnclosingFunction.has_value()) return std::nullopt;
      auto TypeIDX = (*Context.EnclosingFunction)->Type;
      auto Parameters = Context.Module[TypeIDX]->getParamTypes();
      auto const &Locals = (*Context.EnclosingFunction)->Locals;
      auto CastedIndex = static_cast<std::size_t>(Index);
      if (CastedIndex < ranges::size(Parameters))
        return Parameters[CastedIndex];
      CastedIndex = CastedIndex - ranges::size(Parameters);
      if (CastedIndex < ranges::size(Locals)) return Locals[CastedIndex];
      return std::nullopt;
    }
  };

  LabelStack &labels() { return Labels; }
  bool hasReturn() const { return EnclosingFunction.has_value(); }
  auto return_() const {
    auto TypeIDX = (*EnclosingFunction)->Type;
    auto const *Type = Module[TypeIDX];
    return Type->getResultTypes();
  }

  auto types() const { return Wrapper<TypeIDX>{*this}; }
  auto functions() const { return Wrapper<FuncIDX>{*this}; }
  auto tables() const { return Wrapper<TableIDX>{*this}; }
  auto memories() const { return Wrapper<MemIDX>{*this}; }
  auto globals() const { return Wrapper<GlobalIDX>{*this}; }
  auto locals() const { return LocalWrapper{*this}; }

  explicit ExprValidationContext(ModuleView MView)
      : Module(std::move(MView)), EnclosingFunction(std::nullopt) {}
  ExprValidationContext(ModuleView MView, entities::Function const &Function)
      : Module(std::move(MView)), EnclosingFunction(std::addressof(Function)) {}
};
} // namespace

//////////////////////////// ExprValidationVisitor /////////////////////////////
namespace {
class ExprValidationVisitor
    : public InstVisitorBase<
          ExprValidationVisitor, std::unique_ptr<ValidationError>> {
  ExprValidationContext &Context;
  TraceCollector &Trace;
  OperandStack TypeStack;

  template <typename... ArgTypes>
  static constexpr std::array<OperandStackElement, sizeof...(ArgTypes)>
  BuildTypesArray(ArgTypes &&...Args) {
    return {std::forward<ArgTypes>(Args)...};
  }

public:
  ExprValidationVisitor(ExprValidationContext &Context_, TraceCollector &Trace_)
      : Context(Context_), Trace(Trace_) {}

  ErrorPtr visit(InstructionPtr const &InstPtr) {
    using BaseVisitor = InstVisitorBase<
        ExprValidationVisitor, std::unique_ptr<ValidationError>>;
    Trace.pushInstSite(InstPtr);
    if (auto Error = BaseVisitor::visit(InstPtr.asPointer())) return Error;
    Trace.popInstSite();
    return nullptr;
  }

  ExprValidationVisitor duplicate() {
    return ExprValidationVisitor(Context, Trace);
  }

  template <ranges::input_range T, ranges::forward_range U>
  ErrorPtr operator()(
      bytecode::Expression const &Expr, T const &Parameters, U const &Results) {
    using TT = ranges::range_value_type_t<T>;
    using UU = ranges::range_value_type_t<U>;
    static_assert(std::convertible_to<TT, ValueType>);
    static_assert(std::convertible_to<UU, ValueType>);
    static_assert(ranges::sized_range<U>, "for site recovery");
    assert(TypeStack.empty()); // fresh start
    auto AlwaysSuccessful = TypeStack(BuildTypesArray(), Parameters);
    assert(AlwaysSuccessful);
    utility::ignore(AlwaysSuccessful);
    for (auto const &InstPtr : Expr)
      if (auto Error = visit(InstPtr)) return Error;
    if (!TypeStack(Results, BuildTypesArray())) {
      auto Epsilon = TypeStack.getEpsilon();
      auto Actual = TypeStack.recover(ranges::size(Results));
      return Trace.BuildError(Epsilon, Results, Actual);
    }
    if (!TypeStack.empty()) {
      auto Epsilon = TypeStack.getEpsilon();
      auto Actual = TypeStack.recover(TypeStack.size());
      return Trace.BuildError(Epsilon, BuildTypesArray(), Actual);
    }
    return nullptr;
  }

#define ON(Name, ParamTypes, ResultTypes)                                      \
  ErrorPtr operator()(Name const *) {                                          \
    auto Parameters = BuildTypesArray(                                         \
        BOOST_PP_LIST_ENUM(BOOST_PP_ARRAY_TO_LIST(ParamTypes)));               \
    auto Results = BuildTypesArray(                                            \
        BOOST_PP_LIST_ENUM(BOOST_PP_ARRAY_TO_LIST(ResultTypes)));              \
    if (!TypeStack(Parameters, Results)) {                                     \
      auto Actuals = TypeStack.recover(Parameters.size());                     \
      return Trace.BuildError(TypeStack.getEpsilon(), Parameters, Actuals);    \
    }                                                                          \
    return nullptr;                                                            \
  }
  // clang-format off
  /* I32 Comparison Instructions */
  ON(I32Eqz      , (1, (I32)     ), (1, (I32))) // [i32]      -> [i32]
  ON(I32Eq       , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32Ne       , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32LtS      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32LtU      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32GtS      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32GtU      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32LeS      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32LeU      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32GeS      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32GeU      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  /* I64 Comparison Instructions */
  ON(I64Eqz      , (1, (I64     )), (1, (I32))) // [i64]      -> [i32]
  ON(I64Eq       , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  ON(I64Ne       , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  ON(I64LtS      , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  ON(I64LtU      , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  ON(I64GtS      , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  ON(I64GtU      , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  ON(I64LeS      , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  ON(I64LeU      , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  ON(I64GeS      , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  ON(I64GeU      , (2, (I64, I64)), (1, (I32))) // [i64, i64] -> [i32]
  /* F32 Comparison Instructions */
  ON(F32Eq       , (2, (F32, F32)), (1, (I32))) // [f32, f32] -> [i32]
  ON(F32Ne       , (2, (F32, F32)), (1, (I32))) // [f32, f32] -> [i32]
  ON(F32Lt       , (2, (F32, F32)), (1, (I32))) // [f32, f32] -> [i32]
  ON(F32Gt       , (2, (F32, F32)), (1, (I32))) // [f32, f32] -> [i32]
  ON(F32Le       , (2, (F32, F32)), (1, (I32))) // [f32, f32] -> [i32]
  ON(F32Ge       , (2, (F32, F32)), (1, (I32))) // [f32, f32] -> [i32]
  /* F64 Comparison Instructions */
  ON(F64Eq       , (2, (F64, F64)), (1, (I32))) // [f64, f64] -> [i32]
  ON(F64Ne       , (2, (F64, F64)), (1, (I32))) // [f64, f64] -> [i32]
  ON(F64Lt       , (2, (F64, F64)), (1, (I32))) // [f64, f64] -> [i32]
  ON(F64Gt       , (2, (F64, F64)), (1, (I32))) // [f64, f64] -> [i32]
  ON(F64Le       , (2, (F64, F64)), (1, (I32))) // [f64, f64] -> [i32]
  ON(F64Ge       , (2, (F64, F64)), (1, (I32))) // [f64, f64] -> [i32]
  /* I32 Arithmetic Instructions */
  ON(I32Clz      , (1, (I32     )), (1, (I32))) // [i32]      -> [i32]
  ON(I32Ctz      , (1, (I32     )), (1, (I32))) // [i32]      -> [i32]
  ON(I32Popcnt   , (1, (I32     )), (1, (I32))) // [i32]      -> [i32]
  ON(I32Add      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32Sub      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32Mul      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32DivS     , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32DivU     , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32RemS     , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32RemU     , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32And      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32Or       , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32Xor      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32Shl      , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32ShrS     , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32ShrU     , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32Rotl     , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  ON(I32Rotr     , (2, (I32, I32)), (1, (I32))) // [i32, i32] -> [i32]
  /* I64 Arithmetic Instructions */
  ON(I64Clz      , (1, (I64     )), (1, (I64))) // [i64]      -> [i64]
  ON(I64Ctz      , (1, (I64     )), (1, (I64))) // [i64]      -> [i64]
  ON(I64Popcnt   , (1, (I64     )), (1, (I64))) // [i64]      -> [i64]
  ON(I64Add      , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64Sub      , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64Mul      , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64DivS     , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64DivU     , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64RemS     , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64RemU     , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64And      , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64Or       , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64Xor      , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64Shl      , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64ShrS     , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64ShrU     , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64Rotl     , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  ON(I64Rotr     , (2, (I64, I64)), (1, (I64))) // [i64, i64] -> [i64]
  /* F32 Arithmetic Instructions */
  ON(F32Abs      , (1, (F32     )), (1, (F32))) // [f32]      -> [f32]
  ON(F32Neg      , (1, (F32     )), (1, (F32))) // [f32]      -> [f32]
  ON(F32Ceil     , (1, (F32     )), (1, (F32))) // [f32]      -> [f32]
  ON(F32Floor    , (1, (F32     )), (1, (F32))) // [f32]      -> [f32]
  ON(F32Trunc    , (1, (F32     )), (1, (F32))) // [f32]      -> [f32]
  ON(F32Nearest  , (1, (F32     )), (1, (F32))) // [f32]      -> [f32]
  ON(F32Sqrt     , (1, (F32     )), (1, (F32))) // [f32]      -> [f32]
  ON(F32Add      , (2, (F32, F32)), (1, (F32))) // [f32, f32] -> [f32]
  ON(F32Sub      , (2, (F32, F32)), (1, (F32))) // [f32, f32] -> [f32]
  ON(F32Mul      , (2, (F32, F32)), (1, (F32))) // [f32, f32] -> [f32]
  ON(F32Div      , (2, (F32, F32)), (1, (F32))) // [f32, f32] -> [f32]
  ON(F32Min      , (2, (F32, F32)), (1, (F32))) // [f32, f32] -> [f32]
  ON(F32Max      , (2, (F32, F32)), (1, (F32))) // [f32, f32] -> [f32]
  ON(F32CopySign , (2, (F32, F32)), (1, (F32))) // [f32, f32] -> [f32]
  /* F64 Arithmetic Instructions */
  ON(F64Abs      , (1, (F64     )), (1, (F64))) // [f64]      -> [f64]
  ON(F64Neg      , (1, (F64     )), (1, (F64))) // [f64]      -> [f64]
  ON(F64Ceil     , (1, (F64     )), (1, (F64))) // [f64]      -> [f64]
  ON(F64Floor    , (1, (F64     )), (1, (F64))) // [f64]      -> [f64]
  ON(F64Trunc    , (1, (F64     )), (1, (F64))) // [f64]      -> [f64]
  ON(F64Nearest  , (1, (F64     )), (1, (F64))) // [f64]      -> [f64]
  ON(F64Sqrt     , (1, (F64     )), (1, (F64))) // [f64]      -> [f64]
  ON(F64Add      , (2, (F64, F64)), (1, (F64))) // [f64, f64] -> [f64]
  ON(F64Sub      , (2, (F64, F64)), (1, (F64))) // [f64, f64] -> [f64]
  ON(F64Mul      , (2, (F64, F64)), (1, (F64))) // [f64, f64] -> [f64]
  ON(F64Div      , (2, (F64, F64)), (1, (F64))) // [f64, f64] -> [f64]
  ON(F64Min      , (2, (F64, F64)), (1, (F64))) // [f64, f64] -> [f64]
  ON(F64Max      , (2, (F64, F64)), (1, (F64))) // [f64, f64] -> [f64]
  ON(F64CopySign , (2, (F64, F64)), (1, (F64))) // [f64, f64] -> [f64]
  /* Conversion Instructions */
  ON(I32WrapI64       , (1, (I64)), (1, (I32))) // [i64] -> [i32]
  ON(I32TruncF32S     , (1, (F32)), (1, (I32))) // [f32] -> [i32]
  ON(I32TruncF32U     , (1, (F32)), (1, (I32))) // [f32] -> [i32]
  ON(I32TruncF64S     , (1, (F64)), (1, (I32))) // [f64] -> [i32]
  ON(I32TruncF64U     , (1, (F64)), (1, (I32))) // [f64] -> [i32]
  ON(I64ExtendI32S    , (1, (I32)), (1, (I64))) // [i32] -> [i64]
  ON(I64ExtendI32U    , (1, (I32)), (1, (I64))) // [i32] -> [i64]
  ON(I64TruncF32S     , (1, (F32)), (1, (I64))) // [f32] -> [i64]
  ON(I64TruncF32U     , (1, (F32)), (1, (I64))) // [f32] -> [i64]
  ON(I64TruncF64S     , (1, (F64)), (1, (I64))) // [f64] -> [i64]
  ON(I64TruncF64U     , (1, (F64)), (1, (I64))) // [f64] -> [i64]
  ON(F32ConvertI32S   , (1, (I32)), (1, (F32))) // [i32] -> [f32]
  ON(F32ConvertI32U   , (1, (I32)), (1, (F32))) // [i32] -> [f32]
  ON(F32ConvertI64S   , (1, (I64)), (1, (F32))) // [i64] -> [f32]
  ON(F32ConvertI64U   , (1, (I64)), (1, (F32))) // [i64] -> [f32]
  ON(F32DemoteF64     , (1, (F64)), (1, (F32))) // [f64] -> [f32]
  ON(F64ConvertI32S   , (1, (I32)), (1, (F64))) // [i32] -> [f64]
  ON(F64ConvertI32U   , (1, (I32)), (1, (F64))) // [i32] -> [f64]
  ON(F64ConvertI64S   , (1, (I64)), (1, (F64))) // [i64] -> [f64]
  ON(F64ConvertI64U   , (1, (I64)), (1, (F64))) // [i64] -> [f64]
  ON(F64PromoteF32    , (1, (F32)), (1, (F64))) // [f32] -> [f64]
  ON(I32ReinterpretF32, (1, (F32)), (1, (I32))) // [f32] -> [i32]
  ON(I64ReinterpretF64, (1, (F64)), (1, (I64))) // [f64] -> [i64]
  ON(F32ReinterpretI32, (1, (I32)), (1, (F32))) // [i32] -> [f32]
  ON(F64ReinterpretI64, (1, (I64)), (1, (F64))) // [i64] -> [f64]
  /* Numeric Constant Instructions */
  ON(I32Const         , (0, ()   ), (1, (I32))) // [] -> [i32]
  ON(I64Const         , (0, ()   ), (1, (I64))) // [] -> [i64]
  ON(F32Const         , (0, ()   ), (1, (F32))) // [] -> [f32]
  ON(F64Const         , (0, ()   ), (1, (F64))) // [] -> [f64]
  /* Control Instructions */
  ON(Nop              , (0, ()   ), (0, ()   )) // []    -> []
  /* Memory Instruction */
  ON(MemorySize       , (0, ()   ), (1, (I32))) // []    -> [i32]
  ON(MemoryGrow       , (1, (I32)), (1, (I32))) // [i32] -> [i32]
  /* Saturated Conversion Instructions */
  ON(I32TruncSatF32S  , (1, (F32)), (1, (I32))) // [f32] -> [i32]
  ON(I32TruncSatF32U  , (1, (F32)), (1, (I32))) // [f32] -> [i32]
  ON(I32TruncSatF64S  , (1, (F64)), (1, (I32))) // [f64] -> [i32]
  ON(I32TruncSatF64U  , (1, (F64)), (1, (I32))) // [f64] -> [i32]
  ON(I64TruncSatF32S  , (1, (F32)), (1, (I64))) // [f32] -> [i64]
  ON(I64TruncSatF32U  , (1, (F32)), (1, (I64))) // [f32] -> [i64]
  ON(I64TruncSatF64S  , (1, (F64)), (1, (I64))) // [f64] -> [i64]
  ON(I64TruncSatF64U  , (1, (F64)), (1, (I64))) // [f64] -> [i64]
  /* Integer Extension Instructions */
  ON(I32Extend8S      , (1, (I32)), (1, (I32))) // [i32] -> [i32]
  ON(I32Extend16S     , (1, (I32)), (1, (I32))) // [i32] -> [i32]
  ON(I64Extend8S      , (1, (I64)), (1, (I64))) // [i64] -> [i64]
  ON(I64Extend16S     , (1, (I64)), (1, (I64))) // [i64] -> [i64]
  ON(I64Extend32S     , (1, (I64)), (1, (I64))) // [i64] -> [i64]
// clang-format on
#undef ON
#define ON(Name, Width, ParamTypes, ResultTypes)                               \
  ErrorPtr operator()(Name const *Inst) {                                      \
    auto Memory = Context.memories()[static_cast<MemIDX>(0)];                  \
    if (!Memory.has_value())                                                   \
      return Trace.BuildError(MalformedErrorKind::MEM_INDEX_OUT_OF_BOUND);     \
    if (!(Inst->Align <= std::countr_zero(static_cast<unsigned>(Width)) + 1))  \
      return Trace.BuildError(MalformedErrorKind::INVALID_ALIGN);              \
    auto Parameters = BuildTypesArray(                                         \
        BOOST_PP_LIST_ENUM(BOOST_PP_ARRAY_TO_LIST(ParamTypes)));               \
    auto Results = BuildTypesArray(                                            \
        BOOST_PP_LIST_ENUM(BOOST_PP_ARRAY_TO_LIST(ResultTypes)));              \
    if (!TypeStack(Parameters, Results)) {                                     \
      auto Epsilon = TypeStack.getEpsilon();                                   \
      auto Actuals = TypeStack.recover(Parameters.size());                     \
      return Trace.BuildError(Epsilon, Parameters, Actuals);                   \
    }                                                                          \
    return nullptr;                                                            \
  }
  // clang-format off
  ON(I32Load   , 32, (1, (I32     )), (1, (I32)))
  ON(I64Load   , 64, (1, (I32     )), (1, (I64)))
  ON(F32Load   , 32, (1, (I32     )), (1, (F32)))
  ON(F64Load   , 64, (1, (I32     )), (1, (F64)))
  ON(I32Load8S ,  8, (1, (I32     )), (1, (I32)))
  ON(I32Load8U ,  8, (1, (I32     )), (1, (I32)))
  ON(I32Load16S, 16, (1, (I32     )), (1, (I32)))
  ON(I32Load16U, 16, (1, (I32     )), (1, (I32)))
  ON(I64Load8S ,  8, (1, (I32     )), (1, (I64)))
  ON(I64Load8U ,  8, (1, (I32     )), (1, (I64)))
  ON(I64Load16S, 16, (1, (I32     )), (1, (I64)))
  ON(I64Load16U, 16, (1, (I32     )), (1, (I64)))
  ON(I64Load32S, 32, (1, (I32     )), (1, (I64)))
  ON(I64Load32U, 32, (1, (I32     )), (1, (I64)))
  ON(I32Store  , 32, (2, (I32, I32)), (0, (   )))
  ON(I64Store  , 64, (2, (I32, I64)), (0, (   )))
  ON(F32Store  , 32, (2, (I32, F32)), (0, (   )))
  ON(F64Store  , 64, (2, (I32, F64)), (0, (   )))
  ON(I32Store8 ,  8, (2, (I32, I32)), (0, (   )))
  ON(I32Store16, 16, (2, (I32, I32)), (0, (   )))
  ON(I64Store8 ,  8, (2, (I32, I64)), (0, (   )))
  ON(I64Store16, 16, (2, (I32, I64)), (0, (   )))
  ON(I64Store32, 32, (2, (I32, I64)), (0, (   )))
// clang-format on
#undef ON

  ErrorPtr operator()(Unreachable const *);
  ErrorPtr operator()(Return const *);
  ErrorPtr operator()(Drop const *);
  ErrorPtr operator()(Select const *);
  ErrorPtr operator()(Block const *);
  ErrorPtr operator()(Loop const *);
  ErrorPtr operator()(If const *);
  ErrorPtr operator()(Br const *);
  ErrorPtr operator()(BrIf const *);
  ErrorPtr operator()(BrTable const *);
  ErrorPtr operator()(Call const *);
  ErrorPtr operator()(CallIndirect const *);
  ErrorPtr operator()(LocalGet const *);
  ErrorPtr operator()(LocalSet const *);
  ErrorPtr operator()(LocalTee const *);
  ErrorPtr operator()(GlobalGet const *);
  ErrorPtr operator()(GlobalSet const *);

private:
  ErrorPtr validateBlockResult(BlockResultType const &Type);
  FunctionType convertBlockResult(BlockResultType const &Type);
};

ErrorPtr ExprValidationVisitor::operator()(Unreachable const *) {
  // C |- unreachable: [t1*] -> [t2*]
  // always success
  TypeStack.clear();
  TypeStack.setEpsilon();
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(Return const *) {
  //         C.return = [t*]
  // --------------------------------
  //  C |- return: [t1* t*] -> [t2*]
  if (!Context.hasReturn())
    return Trace.BuildError(MalformedErrorKind::MISSING_CONTEXT_RETURN);
  if (!TypeStack(Context.return_(), BuildTypesArray())) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(ranges::size(Context.return_()));
    return Trace.BuildError(Epsilon, Context.return_(), Actual);
  }
  TypeStack.clear();
  TypeStack.setEpsilon();
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(Drop const *) {
  // C |- drop: [t] -> []
  do {
    auto T = TypeStack.peek();
    if (!T.has_value()) break;
    auto AlwaysSuccessful = TypeStack(BuildTypesArray(*T), BuildTypesArray());
    assert(AlwaysSuccessful);
    utility::ignore(AlwaysSuccessful);
    return nullptr;
  } while (false);
  auto Epsilon = TypeStack.getEpsilon();
  auto Actual = TypeStack.recover(1);
  auto Expect = BuildTypesArray(TypeVariable(0));
  return Trace.BuildError(Epsilon, Expect, Actual);
}

ErrorPtr ExprValidationVisitor::operator()(Select const *) {
  // C |- select: [t t i32] -> [t]
  do {
    auto T = TypeStack.peek(1);
    if (!T.has_value()) break;
    if (!TypeStack(BuildTypesArray(*T, *T, I32), BuildTypesArray(*T))) break;
    return nullptr;
  } while (false);
  auto Epsilon = TypeStack.getEpsilon();
  auto Actual = TypeStack.recover(3);
  auto Expect = BuildTypesArray(TypeVariable(0), TypeVariable(0), I32);
  return Trace.BuildError(Epsilon, Expect, Actual);
}

ErrorPtr
ExprValidationVisitor::validateBlockResult(BlockResultType const &Type) {
  if (std::holds_alternative<TypeIDX>(Type)) {
    auto FunctionTypePtr = Context.types()[std::get<TypeIDX>(Type)];
    if (!FunctionTypePtr.has_value())
      return Trace.BuildError(MalformedErrorKind::TYPE_INDEX_OUT_OF_BOUND);
    assert(*FunctionTypePtr != nullptr);
    assert(validate(**FunctionTypePtr));
    return nullptr;
  }
  if (std::holds_alternative<ValueType>(Type)) {
    if (!validate(std::get<ValueType>(Type)))
      return Trace.BuildError(MalformedErrorKind::MALFORMED_VALUE_TYPE);
    return nullptr;
  }
  if (std::holds_alternative<BlockResultUnit>(Type)) return nullptr;
  SABLE_UNREACHABLE();
}

FunctionType
ExprValidationVisitor::convertBlockResult(BlockResultType const &Type) {
  if (std::holds_alternative<TypeIDX>(Type)) {
    auto FunctionTypePtr = Context.types()[std::get<TypeIDX>(Type)];
    assert(FunctionTypePtr.has_value());
    assert(*FunctionTypePtr != nullptr);
    return **FunctionTypePtr;
  }
  if (std::holds_alternative<ValueType>(Type)) {
    std::array<ValueType, 0> Parameters{};
    std::array<ValueType, 1> Results{std::get<ValueType>(Type)};
    return FunctionType(Parameters, Results);
  }
  if (std::holds_alternative<BlockResultUnit>(Type)) {
    std::array<ValueType, 0> Empty{};
    return FunctionType(Empty, Empty);
  }
  SABLE_UNREACHABLE();
}

ErrorPtr ExprValidationVisitor::operator()(Block const *Inst) {
  //      C |- blocktype: [t1*] -> [t2*]
  //      C, labels[t2*] |- instr*: [t1*] -> [t2*]
  // -------------------------------------------------
  //  C |- block blocktype instr* end: [t1*] -> [t2*]
  if (auto Error = validateBlockResult(Inst->Type)) return Error;
  auto ConvertedType = convertBlockResult(Inst->Type);
  auto Parameters = ConvertedType.getParamTypes();
  auto Results = ConvertedType.getResultTypes();
  Context.labels().push(Results);
  if (!TypeStack(Parameters, Results)) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(ranges::size(Parameters));
    return Trace.BuildError(Epsilon, Parameters, Actual);
  }
  if (auto Error = duplicate()(Inst->Body, Parameters, Results)) return Error;
  Context.labels().pop();
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(Loop const *Inst) {
  //     C |- blocktype: [t1*] -> [t2*]
  //     C, labels[t1*] |- instr*: [t1*] -> [t2*]
  // ------------------------------------------------
  //  C |- loop blocktype instr* end: [t1*] -> [t2*]
  if (auto Error = validateBlockResult(Inst->Type)) return Error;
  auto ConvertedType = convertBlockResult(Inst->Type);
  auto Parameters = ConvertedType.getParamTypes();
  auto Results = ConvertedType.getResultTypes();
  Context.labels().push(Parameters);
  if (!TypeStack(Parameters, Results)) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(ranges::size(Parameters));
    return Trace.BuildError(Epsilon, Parameters, Actual);
  }
  if (auto Error = duplicate()(Inst->Body, Parameters, Results)) return Error;
  Context.labels().pop();
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(If const *Inst) {
  //           C |- blocktype: [t1*] -> [t2*]
  //           C, labels[t2*] |- instr1*: [t1*] -> [t2*]
  //           C, labels[t2*] |- instr2*: [t1*] -> [tr*]
  // ----------------------------------------------------------------
  //  C |- if blocktype instr1* else instr2* end: [t1* i32] -> [t2*]
  if (auto Error = validateBlockResult(Inst->Type)) return Error;
  auto ConvertedType = convertBlockResult(Inst->Type);
  auto Parameters = ConvertedType.getParamTypes();
  auto Results = ConvertedType.getResultTypes();
  Context.labels().push(Results);
  auto IfParameters =
      ranges::views::concat(Parameters, ranges::views::single(I32));
  if (!TypeStack(IfParameters, Results)) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(ranges::size(IfParameters));
    return Trace.BuildError(Epsilon, IfParameters, Actual);
  }
  if (auto Error = duplicate()(Inst->True, Parameters, Results)) return Error;
  if (Inst->False.has_value())
    if (auto Error = duplicate()(*Inst->False, Parameters, Results))
      return Error;
  Context.labels().pop();
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(Br const *Inst) {
  //       C.labels[l] = [t*]
  // ------------------------------
  //  C |- br l: [t1* t*] -> [t2*]
  auto Types = Context.labels()[Inst->Target];
  if (!Types.has_value())
    return Trace.BuildError(MalformedErrorKind::LABEL_INDEX_OUT_OF_BOUND);
  if (!TypeStack(*Types, BuildTypesArray())) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(ranges::size(*Types));
    return Trace.BuildError(Epsilon, *Types, Actual);
  }
  TypeStack.clear();
  TypeStack.setEpsilon();
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(BrIf const *Inst) {
  //       C.labels[l] = [t*]
  // --------------------------------
  //  C |- br_if l: [t* i32] -> [t*]
  auto Types = Context.labels()[Inst->Target];
  if (!Types.has_value())
    return Trace.BuildError(MalformedErrorKind::LABEL_INDEX_OUT_OF_BOUND);
  auto BrIfParameters =
      ranges::views::concat(*Types, ranges::views::single(I32));
  if (!TypeStack(BrIfParameters, *Types)) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(ranges::size(BrIfParameters));
    return Trace.BuildError(Epsilon, BrIfParameters, Actual);
  }
  return nullptr;
}

namespace {
template <ranges::input_range T, ranges::input_range U>
bool rangeEqual(T const &LHS, U const &RHS) {
  static_assert(ranges::sized_range<T> && ranges::sized_range<U>);
  if (ranges::size(LHS) != ranges::size(RHS)) return false;
  auto LHSBegin = ranges::begin(LHS);
  auto RHSBegin = ranges::begin(RHS);
  while (LHSBegin != ranges::end(LHS)) {
    if (*LHSBegin == *RHSBegin) return false;
    ++LHSBegin, ++RHSBegin;
  }
  return true;
}
} // namespace

ErrorPtr ExprValidationVisitor::operator()(BrTable const *Inst) {
  //  (C.labels[l] = [t*])*   C.labels[ln] = [t*]
  // ---------------------------------------------
  //  C |- br_table l* ln: [t1* t* i32] -> [t2*]
  auto DefaultTypes = Context.labels()[Inst->DefaultTarget];
  if (!DefaultTypes.has_value())
    return Trace.BuildError(MalformedErrorKind::LABEL_INDEX_OUT_OF_BOUND);
  for (auto const &Index : Inst->Targets) {
    auto Types = Context.labels()[Index];
    if (!Types.has_value())
      return Trace.BuildError(MalformedErrorKind::LABEL_INDEX_OUT_OF_BOUND);
    if (!rangeEqual(*Types, *DefaultTypes))
      return Trace.BuildError(MalformedErrorKind::INVALID_BRANCH_TABLE);
  }
  auto BrTableParameters =
      ranges::views::concat(*DefaultTypes, ranges::views::single(I32));
  if (!TypeStack(BrTableParameters, BuildTypesArray())) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(ranges::size(BrTableParameters));
    return Trace.BuildError(Epsilon, BrTableParameters, Actual);
  }
  TypeStack.clear();
  TypeStack.setEpsilon();
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(Call const *Inst) {
  //  C.funcs[x] = [t1*] -> [t2*]
  // -----------------------------
  //  C |- call x: [t1*] -> [t2*]
  auto Function = Context.functions()[Inst->Target];
  if (!Function.has_value())
    return Trace.BuildError(MalformedErrorKind::FUNC_INDEX_OUT_OF_BOUND);
  auto Parameters = (*Function).getType() /* FunctionType * */->getParamTypes();
  auto Results = (*Function).getType() /* FunctionType * */->getResultTypes();
  if (!TypeStack(Parameters, Results)) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(ranges::size(Parameters));
    return Trace.BuildError(Epsilon, Parameters, Actual);
  }
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(CallIndirect const *Inst) {
  //  C.tables[0] = limits funcref    C.types[x] = [t1*] -> [t2*]
  // -------------------------------------------------------------
  //         C |- call_indirect x: [t1* i32] -> [t2*]
  auto Table = Context.tables()[static_cast<TableIDX>(0)];
  if (!Table.has_value())
    return Trace.BuildError(MalformedErrorKind::TABLE_INDEX_OUT_OF_BOUND);
  auto Type = Context.types()[Inst->Type];
  if (!Type.has_value())
    return Trace.BuildError(MalformedErrorKind::TYPE_INDEX_OUT_OF_BOUND);
  auto Parameters = (*Type) /* FunctionType * */->getParamTypes();
  auto Results = (*Type) /* FunctionType * */->getResultTypes();
  auto CallIndirectParameters =
      ranges::views::concat(Parameters, ranges::views::single(I32));
  if (!TypeStack(CallIndirectParameters, Results)) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(ranges::size(CallIndirectParameters));
    return Trace.BuildError(Epsilon, CallIndirectParameters, Actual);
  }
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(LocalGet const *Inst) {
  //       C.locals[x] = t
  // -----------------------------
  //  C |- local.get x: [] -> [t]
  auto Local = Context.locals()[Inst->Target];
  if (!Local.has_value())
    return Trace.BuildError(MalformedErrorKind::LOCAL_INDEX_OUT_OF_BOUND);
  auto AlwaysSuccessful =
      TypeStack(BuildTypesArray(), ranges::views::single(*Local));
  assert(AlwaysSuccessful);
  utility::ignore(AlwaysSuccessful);
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(LocalSet const *Inst) {
  //       C.locals[x] = t
  // -----------------------------
  //  C |- local.set x: [t] -> []
  auto Local = Context.locals()[Inst->Target];
  if (!Local.has_value())
    return Trace.BuildError(MalformedErrorKind::LOCAL_INDEX_OUT_OF_BOUND);
  if (!TypeStack(ranges::views::single(*Local), BuildTypesArray())) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(1);
    auto Expecting = ranges::views::single(*Local);
    return Trace.BuildError(Epsilon, Expecting, Actual);
  }
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(LocalTee const *Inst) {
  //       C.locals[x] = t
  // ------------------------------
  //  C |- local.tee x: [t] -> [t]
  auto Local = Context.locals()[Inst->Target];
  if (!Local.has_value())
    return Trace.BuildError(MalformedErrorKind::LOCAL_INDEX_OUT_OF_BOUND);
  auto ParameterAndResult = ranges::views::single(*Local);
  if (!TypeStack(ParameterAndResult, ParameterAndResult)) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(1);
    return Trace.BuildError(Epsilon, ParameterAndResult, Actual);
  }
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(GlobalGet const *Inst) {
  //        C.globals[x] = t
  // ------------------------------
  //  C |- global.get x: [] -> [t]
  auto Global = Context.globals()[Inst->Target];
  if (!Global.has_value())
    return Trace.BuildError(MalformedErrorKind::GLOBAL_INDEX_OUT_OF_BOUND);
  auto Type = (*Global).getType() /* GlobalType const * */->getType();
  auto AlwaysSuccessful =
      TypeStack(BuildTypesArray(), ranges::views::single(Type));
  assert(AlwaysSuccessful);
  utility::ignore(AlwaysSuccessful);
  return nullptr;
}

ErrorPtr ExprValidationVisitor::operator()(GlobalSet const *Inst) {
  //     C.globals[x] = var t
  // ------------------------------
  //  C |- global.set x: [t] -> []
  auto Global = Context.globals()[Inst->Target];
  if (!Global.has_value())
    return Trace.BuildError(MalformedErrorKind::GLOBAL_INDEX_OUT_OF_BOUND);
  if (!(*Global).getType()->isVar())
    return Trace.BuildError(MalformedErrorKind::GLOBAL_MUST_BE_MUT);
  auto Type = (*Global).getType() /* GlobalType const * */->getType();
  auto Parameters = ranges::views::single(Type);
  if (!TypeStack(Parameters, BuildTypesArray())) {
    auto Epsilon = TypeStack.getEpsilon();
    auto Actual = TypeStack.recover(1);
    return Trace.BuildError(Epsilon, Parameters, Actual);
  }
  return nullptr;
}
} // namespace

//////////////////////////////// TraceCollector ////////////////////////////////
void TraceCollector::enterEntity(EntitySiteKind ESiteKind_, std::size_t N) {
  ESiteKind = ESiteKind_;
  ESiteIndex = N;
  ISites.clear();
}
// clang-format off
void TraceCollector::enterType(std::size_t N) 
{ enterEntity(EntitySiteKind::Type, N); }
void TraceCollector::enterFunction(std::size_t N)
{ enterEntity(EntitySiteKind::Function, N); }
void TraceCollector::enterTable(std::size_t N)
{ enterEntity(EntitySiteKind::Table, N); }
void TraceCollector::enterMemory(std::size_t N)
{ enterEntity(EntitySiteKind::Memory, N); }
void TraceCollector::enterGlobal(std::size_t N)
{ enterEntity(EntitySiteKind::Global, N); }
void TraceCollector::enterElement(std::size_t N)
{ enterEntity(EntitySiteKind::Element, N); }
void TraceCollector::enterData(std::size_t N)
{ enterEntity(EntitySiteKind::Data, N); }
void TraceCollector::enterImport(std::size_t N)
{ enterEntity(EntitySiteKind::Import, N); }
void TraceCollector::enterExport(std::size_t N)
{ enterEntity(EntitySiteKind::Export, N); }
// clang-format on

void TraceCollector::pushInstSite(InstructionPtr const &InstPtr) {
  ISites.push_back(std::addressof(InstPtr));
}
void TraceCollector::popInstSite() {
  assert(!ISites.empty());
  ISites.pop_back();
}

/////////// std::unique_ptr<ValidationError> validate(Module const &) //////////
namespace {
namespace detail {
class ImportValidationVisitor {
  TraceCollector &Trace;
  std::size_t NumTypes;

public:
  ImportValidationVisitor(TraceCollector &Trace_, Module const &M)
      : Trace(Trace_), NumTypes(ranges::size(M.Types)) {}
  ErrorPtr operator()(TypeIDX const &N) {
    auto CastedIndex = static_cast<std::size_t>(N);
    if (!(CastedIndex < NumTypes))
      return Trace.BuildError(MalformedErrorKind::TYPE_INDEX_OUT_OF_BOUND);
    return nullptr;
  }
  ErrorPtr operator()(TableType const &Type) {
    if (!validate(Type))
      return Trace.BuildError(MalformedErrorKind::MALFORMED_TABLE_TYPE);
    return nullptr;
  }
  ErrorPtr operator()(MemoryType const &Type) {
    if (!validate(Type))
      return Trace.BuildError(MalformedErrorKind::MALFORMED_MEMORY_TYPE);
    return nullptr;
  }
  ErrorPtr operator()(GlobalType const &Type) {
    if (!validate(Type))
      return Trace.BuildError(MalformedErrorKind::MALFORMED_GLOBAL_TYPE);
    return nullptr;
  }
};
} // namespace detail

ErrorPtr validateImports(TraceCollector &Trace, Module const &M) {
  detail::ImportValidationVisitor Visitor(Trace, M);
  auto EnumerateView = ranges::views::enumerate(M.Imports);
  for (auto const &[Index, Import] : EnumerateView) {
    Trace.enterImport(Index);
    if (auto Error = std::visit(Visitor, Import.Descriptor)) return Error;
  }
  return nullptr;
}

namespace detail {
class ExportValidationVisitor {
  TraceCollector &Trace;
  std::size_t NumFuncs, NumTables, NumMemories, NumGlobals;

  template <typename T, typename C>
  std::size_t countEntity(Module const &M, C const &Container) {
    return ranges::size(Container) +
           ranges::count_if(M.Imports, [](entities::Import const &Import) {
             return std::holds_alternative<T>(Import.Descriptor);
           });
  }

  template <typename T>
  ErrorPtr
  check(T const &Index, std::size_t Bound, MalformedErrorKind EKind) const {
    auto CastedIndex = static_cast<std::size_t>(Index);
    if (!(CastedIndex < Bound)) return Trace.BuildError(EKind);
    return nullptr;
  }

public:
  ExportValidationVisitor(TraceCollector &Trace_, Module const &M)
      : Trace(Trace_), NumFuncs(countEntity<TypeIDX>(M, M.Functions)),
        NumTables(countEntity<TableType>(M, M.Tables)),
        NumMemories(countEntity<MemoryType>(M, M.Memories)),
        NumGlobals(countEntity<GlobalType>(M, M.Globals)) {}

  ErrorPtr operator()(FuncIDX const &N) const {
    return check(N, NumFuncs, MalformedErrorKind::FUNC_INDEX_OUT_OF_BOUND);
  }
  ErrorPtr operator()(TableIDX const &N) const {
    return check(N, NumTables, MalformedErrorKind::TABLE_INDEX_OUT_OF_BOUND);
  }
  ErrorPtr operator()(MemIDX const &N) const {
    return check(N, NumMemories, MalformedErrorKind::MEM_INDEX_OUT_OF_BOUND);
  }
  ErrorPtr operator()(GlobalIDX const &N) const {
    return check(N, NumGlobals, MalformedErrorKind::GLOBAL_INDEX_OUT_OF_BOUND);
  }
};
} // namespace detail

ErrorPtr validateExports(TraceCollector &Trace, Module const &M) {
  detail::ExportValidationVisitor Visitor(Trace, M);
  auto EnumerateView = ranges::views::enumerate(M.Exports);
  for (auto const &[Index, Export] : EnumerateView) {
    Trace.enterExport(Index);
    if (auto Error = std::visit(Visitor, Export.Descriptor)) return Error;
  }
  return nullptr;
}

ErrorPtr validateTypes(TraceCollector &Trace, Module const &M) {
  auto EnumerateView = ranges::views::enumerate(M.Types);
  for (auto const &[Index, Type] : EnumerateView) {
    Trace.enterType(Index);
    if (!validate(Type))
      return Trace.BuildError(MalformedErrorKind::MALFORMED_FUNCTION_TYPE);
  }
  return nullptr;
}

ErrorPtr validateTables(TraceCollector &Trace, Module const &M) {
  auto EnumerateView = ranges::views::enumerate(M.Tables);
  for (auto const &[Index, Table] : EnumerateView) {
    Trace.enterTable(Index);
    if (!validate(Table.Type))
      return Trace.BuildError(MalformedErrorKind::MALFORMED_TABLE_TYPE);
  }
  return nullptr;
}

ErrorPtr validateMemories(TraceCollector &Trace, Module const &M) {
  auto EnumerateView = ranges::views::enumerate(M.Memories);
  for (auto const &[Index, Memory] : EnumerateView) {
    Trace.enterMemory(Index);
    if (!validate(Memory.Type))
      return Trace.BuildError(MalformedErrorKind::MALFORMED_MEMORY_TYPE);
  }
  return nullptr;
}

} // namespace

ErrorPtr validate(Module const &M) {
  /* validate import section and export section before construct the view, as
   * the indices are assumed to be in-range in the ModuleView constructor */
  TraceCollector Trace;
  if (auto Error = validateImports(Trace, M)) return Error;
  if (auto Error = validateExports(Trace, M)) return Error;
  if (auto Error = validateTypes(Trace, M)) return Error;
  if (auto Error = validateTables(Trace, M)) return Error;
  if (auto Error = validateMemories(Trace, M)) return Error;

  return nullptr;
}
} // namespace bytecode::validation
