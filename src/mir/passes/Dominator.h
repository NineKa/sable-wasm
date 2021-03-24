#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_DOMINATOR
#define SABLE_INCLUDE_GUARD_MIR_PASSES_DOMINATOR

#include "../Function.h"
#include "Pass.h"

#include <memory>
#include <vector>

namespace mir::passes {

// A simple iterative dataflow approach to compute CFG dominators
// TODO: use Tarjan's algorithm

class DominatorPassResult {
public:
  using BasicBlockSet = std::vector<mir::BasicBlock const *>;
  using DominatorMap =
      std::unordered_map<mir::BasicBlock const *, BasicBlockSet>;

private:
  std::shared_ptr<DominatorMap> Dominator;

public:
  explicit DominatorPassResult(std::shared_ptr<DominatorMap> Dominator_);
  std::span<mir::BasicBlock const *const> get(mir::BasicBlock const &BB) const;
  bool dominate(mir::BasicBlock const &V, mir::BasicBlock const &U) const;
};

class DominatorPass {
  std::shared_ptr<DominatorPassResult::DominatorMap> Dominator;
  std::unique_ptr<DominatorPassResult::BasicBlockSet> N;
  mir::Function const *Function;

public:
  void prepare(mir::Function const &Function_);
  PassStatus run();
  void finalize();

  using AnalysisResult = DominatorPassResult;
  AnalysisResult getResult() const;

  static constexpr bool isConstantPass() { return true; }
  static constexpr bool isSingleRunPass() { return false; }
  bool isSkipped(mir::Function const &Function_) const;
};

static_assert(function_pass<DominatorPass>);
} // namespace mir::passes

#endif