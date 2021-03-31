#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_SIMPLIFY_CFG
#define SABLE_INCLUDE_GUARD_MIR_PASSES_SIMPLIFY_CFG

#include "../Function.h"
#include "Pass.h"

namespace mir::passes {
class SimplifyCFGPass {
  mir::Function *Function;

  bool simplifyTrivialPhi(mir::BasicBlock &BasicBlock);
  bool simplifyTrivialBranch(mir::BasicBlock &BasicBlock);
  bool deadBasicBlockElem(mir::BasicBlock &BasicBlock);
  bool deadInstructionElem(mir::BasicBlock &BasicBlock);

public:
  void prepare(mir::Function &Function_);
  PassStatus run();
  void finalize();

  bool isSkipped(mir::Function const &Function_) const;
  using AnalysisResult = void;
  void getResult() const;

  static constexpr bool isConstantPass() { return false; }
  static constexpr bool isSingleRunPass() { return false; }
};

static_assert(function_pass<SimplifyCFGPass>);
} // namespace mir::passes

#endif