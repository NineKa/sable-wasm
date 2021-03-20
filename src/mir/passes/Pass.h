#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_PASS
#define SABLE_INCLUDE_GUARD_MIR_PASSES_PASS

#include "../Module.h"

#include <type_traits>

namespace mir::passes {
enum class PassResult { Converged, InProgress };

// clang-format off
template <typename T> concept function_pass = requires(T Pass) {
  { Pass.run(std::declval<mir::Function &>()) }
  -> std::convertible_to<PassResult>;
  { Pass.isSkipped(std::declval<mir::Function &>()) } 
  -> std::convertible_to<bool>;
  typename T::AnalysisResult;
  { Pass.getResult() } ->std::convertible_to<typename T::AnalysisResult>;
};

template<typename T> concept module_pass = requires(T Pass) {
  { Pass.run(std::declval<mir::Module &>()) }
  -> std::convertible_to<PassResult>;
  typename T::AnalysisResult;
  { Pass.getResult() } -> std::convertible_to<typename T::AnalysisResult>;
};
// clang-format on
} // namespace mir::passes

#endif
