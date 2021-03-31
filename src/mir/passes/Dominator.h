#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_DOMINATOR
#define SABLE_INCLUDE_GUARD_MIR_PASSES_DOMINATOR

#include "../Function.h"
#include "Pass.h"

#include <range/v3/view/filter.hpp>

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

  using DomView = std::span<mir::BasicBlock const *const>;
  DomView getDom(mir::BasicBlock const &BB) const;
  mir::BasicBlock const *getImmediateDom(mir::BasicBlock const &BB) const;

  class DomTreeNode {
    mir::BasicBlock const *BasicBlock;
    std::vector<std::shared_ptr<DomTreeNode>> Children;
    friend class DominatorPassResult;
    void addChildren(std::shared_ptr<DomTreeNode> Child);

  public:
    explicit DomTreeNode(mir::BasicBlock const *BasicBlock_);
    mir::BasicBlock const *get() const;

    using iterator = decltype(Children)::const_iterator;
    iterator begin() const;
    iterator end() const;
    auto getChildren() const { return ranges::make_subrange(begin(), end()); }

    std::vector<mir::BasicBlock const *> asPreorder() const;
    std::vector<mir::BasicBlock const *> asPostorder() const;
  };

  std::shared_ptr<DomTreeNode>
  buildDomTree(mir::BasicBlock const &EntryBB) const;

  bool dominate(BasicBlock const &V, BasicBlock const &U) const;
  bool strictlyDominate(BasicBlock const &V, BasicBlock const &U) const;
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
