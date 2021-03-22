#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_PASS
#define SABLE_INCLUDE_GUARD_MIR_PASSES_PASS

#include "../Module.h"

#include <concepts>
#include <type_traits>

namespace mir::passes {
enum class PassStatus { Converged, InProgress };

// clang-format off
template <typename T> concept function_pass = requires(T Pass) {
  { Pass.prepare(std::declval<mir::Function &>()) };
  { Pass.run() } -> std::convertible_to<PassStatus>;
  { Pass.finalize() };
  { Pass.isSkipped(std::declval<mir::Function &>()) } 
  -> std::convertible_to<bool>;
  typename T::AnalysisResult;
  { Pass.getResult() } ->std::convertible_to<typename T::AnalysisResult>;
  { T::isConstantPass()  } -> std::convertible_to<bool>;
  { T::isSingleRunPass() } -> std::convertible_to<bool>;
};

template<typename T> concept module_pass = requires(T Pass) {
  { Pass.prepare(std::declval<mir::Module &>()) };
  { Pass.run() } -> std::convertible_to<PassStatus>;
  { Pass.finalize() };
  typename T::AnalysisResult;
  { Pass.getResult() } -> std::convertible_to<typename T::AnalysisResult>;
  { T::isConstantPass()  } -> std::convertible_to<bool>;
  { T::isSingleRunPass() } -> std::convertible_to<bool>;
};
// clang-format on

template <module_pass T> struct SimpleModulePassDriver {
  T Pass;

public:
  using AnalysisResult = typename T::AnalysisResult;
  template <
      typename... ArgTypes,
      typename = std::enable_if_t<std::is_constructible_v<T, ArgTypes...>>>
  explicit SimpleModulePassDriver(ArgTypes &&...Args)
      : Pass(std::forward<ArgTypes>(Args)...) {}

  typename T::AnalysisResult operator()(mir::Module &Module) {
    Pass.prepare(Module);
    while (Pass.run() != PassStatus::Converged) {}
    Pass.finalize();
    return Pass.getResult();
  }
};

template <function_pass T> struct SimpleFunctionPassDriver {
  T Pass;

public:
  using AnalysisResult = typename T::AnalysisResult;
  template <
      typename... ArgTypes,
      typename = std::enable_if_t<std::is_constructible_v<T, ArgTypes...>>>
  explicit SimpleFunctionPassDriver(ArgTypes &&...Args)
      : Pass(std::forward<ArgTypes>(Args)...) {}

  typename T::AnalysisResult operator()(mir::Function &Function) {
    Pass.prepare(Function);
    while (Pass.run() != PassStatus::Converged) {}
    Pass.finalize();
    return Pass.getResult();
  }
};
} // namespace mir::passes

#endif
