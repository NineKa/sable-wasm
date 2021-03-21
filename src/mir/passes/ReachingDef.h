#ifndef SABLE_INCLUDE_MIR_PASSES_REACHING_DEF
#define SABLE_INCLUDE_MIR_PASSES_REACHING_DEF

#include "../BasicBlock.h"
#include "../Instruction.h"
#include "../Module.h"
#include "Pass.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/iterator_range.h>

#include <memory>
#include <set>
#include <span>
#include <vector>

namespace mir::passes {

// Reaching Definition Analysis
// MIR has SSA form, thus, the reaching-def analysis is trivial.
// This is used when perform mir SSA validation.
// The implementation is inefficient, and it is designed to be rarely used.
class ReachingDefPassResult {
public:
  using DefSet = llvm::DenseSet<Instruction *>;
  using DefSetView = llvm::iterator_range<typename DefSet::const_iterator>;
  using InMap = std::unordered_map<BasicBlock const *, DefSet>;
  using OutMap = std::unordered_map<BasicBlock const *, DefSet>;

private:
  std::shared_ptr<InMap> Ins;
  std::shared_ptr<OutMap> Outs;

public:
  ReachingDefPassResult(
      std::shared_ptr<InMap> Ins_, std::shared_ptr<OutMap> Outs_);
  DefSetView in(BasicBlock const &BB) const;
  DefSetView out(BasicBlock const &BB) const;
};

class ReachingDefPass {
  std::shared_ptr<ReachingDefPassResult::InMap> Ins;
  std::shared_ptr<ReachingDefPassResult::OutMap> Outs;
  mir::Function *Function;

  bool run(mir::BasicBlock *BasicBlock);
  ReachingDefPassResult::DefSet &getIn(BasicBlock const &BB);
  ReachingDefPassResult::DefSet &getOut(BasicBlock const &BB);

public:
  void prepare(mir::Function &Function);
  PassStatus run();
  void finalize();

  bool isSkipped(mir::Function const &Function_);
  using AnalysisResult = ReachingDefPassResult;
  AnalysisResult getResult() const;

  static bool isConstantPass() { return true; }
  static bool isSingleRunPass() { return false; }
};

static_assert(function_pass<ReachingDefPass>);
} // namespace mir::passes

#endif