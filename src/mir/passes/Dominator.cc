#include "Dominator.h"

#include "../Module.h"

#include <range/v3/algorithm/binary_search.hpp>
#include <range/v3/algorithm/equal_range.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>

#include <iterator>
#include <unordered_map>

namespace mir::passes {

DominatorPassResult::DominatorPassResult(
    std::shared_ptr<DominatorMap> Dominator_)
    : Dominator(std::move(Dominator_)) {}

std::span<mir::BasicBlock const *const>
DominatorPassResult::getDom(BasicBlock const &BB) const {
  auto SearchIter = Dominator->find(std::addressof(BB));
  assert(SearchIter != Dominator->end());
  return std::get<1>(*SearchIter);
}

mir::BasicBlock const *
DominatorPassResult::getImmediateDom(mir::BasicBlock const &BB) const {
  mir::BasicBlock const *ImmediateDom = nullptr;
  auto StrictlyDom = getStrictlyDom(BB);
  for (auto const *Candidate : StrictlyDom) {
    // clang-format off
    auto Others = StrictlyDom
      | ranges::views::filter([&](mir::BasicBlock const *Candidate_) {
          return Candidate_ != Candidate;
        });
    // clang-format on
    auto SearchIter =
        ranges::find_if(Others, [&](mir::BasicBlock const *Candidate_) {
          return strictlyDominate(*Candidate, *Candidate_);
        });
    if (SearchIter == ranges::end(Others)) {
      assert(ImmediateDom == nullptr);
      ImmediateDom = Candidate;
    }
  }
  return ImmediateDom;
}

DominatorPassResult::DomTreeNode::DomTreeNode(mir::BasicBlock const *BB_)
    : BasicBlock(BB_) {}

void DominatorPassResult::DomTreeNode::addChildren(
    std::shared_ptr<DomTreeNode> Node) {
  Children.push_back(std::move(Node));
}

DominatorPassResult::DomTreeNode::iterator
DominatorPassResult::DomTreeNode::begin() const {
  return Children.begin();
}

DominatorPassResult::DomTreeNode::iterator
DominatorPassResult::DomTreeNode::end() const {
  return Children.end();
}

std::shared_ptr<DominatorPassResult::DomTreeNode>
DominatorPassResult::buildDomTree(mir::BasicBlock const &EntryBB) const {
  std::unordered_map<mir::BasicBlock const *, std::shared_ptr<DomTreeNode>>
      NodesMap;
  for (auto const &[BBPtr, Dom] : *Dominator) {
    utility::ignore(Dom);
    auto Node = std::make_shared<DomTreeNode>(BBPtr);
    NodesMap.insert(std::make_pair(BBPtr, std::move(Node)));
  }
  for (auto const &[BBPtr, Dom] : *Dominator) {
    utility::ignore(Dom);
    auto const *ImmediateDom = getImmediateDom(*BBPtr);
    if (ImmediateDom == nullptr) continue;
    auto NodePtr = std::get<1>(*NodesMap.find(BBPtr));
    auto ImmediateDomNodePtr = std::get<1>(*NodesMap.find(ImmediateDom));
    ImmediateDomNodePtr->addChildren(std::move(NodePtr));
  }
  return std::get<1>(*NodesMap.find(std::addressof(EntryBB)));
}

bool DominatorPassResult::dominate(
    mir::BasicBlock const &V, mir::BasicBlock const &U) const {
  auto Dom = getDom(U);
  return ranges::binary_search(Dom, std::addressof(V));
}

bool DominatorPassResult::strictlyDominate(
    mir::BasicBlock const &V, mir::BasicBlock const &U) const {
  auto StrictlyDom = getStrictlyDom(U);
  return ranges::binary_search(StrictlyDom, std::addressof(V));
}

void DominatorPass::prepare(mir::Function const &Function_) {
  using BasicBlockSet = DominatorPassResult::BasicBlockSet;
  using DominatorMap = DominatorPassResult::DominatorMap;
  assert(Function_.hasBody());
  Function = std::addressof(Function_);
  Dominator = std::make_shared<DominatorMap>();
  N = std::make_unique<BasicBlockSet>();
  N->reserve(Function->getBasicBlocks().size());

  for (auto const &BasicBlock : Function->getBasicBlocks())
    N->push_back(std::addressof(BasicBlock));
  ranges::sort(*N);

  for (auto const &BasicBlock : Function->getBasicBlocks()) {
    auto *BasicBlockPtr = std::addressof(BasicBlock);
    if (BasicBlockPtr->isEntryBlock()) {
      Dominator->emplace(BasicBlockPtr, BasicBlockSet{BasicBlockPtr});
    } else {
      Dominator->emplace(BasicBlockPtr, *N);
    }
  }
}

PassStatus DominatorPass::run() {
  bool Changed = false;
  for (auto const &BasicBlock : Function->getBasicBlocks()) {
    auto *BasicBlockPtr = std::addressof(BasicBlock);
    auto Predecessors = BasicBlock.getInwardFlow();
    auto CurDom =
        (Predecessors.empty()) ? DominatorPassResult::BasicBlockSet{} : *N;
    for (auto const *Predecessor : Predecessors) {
      auto const &PreDom = std::get<1>(*Dominator->find(Predecessor));
      DominatorPassResult::BasicBlockSet CurDom_;
      CurDom_.reserve(CurDom.size());
      ranges::set_intersection(CurDom, PreDom, std::back_inserter(CurDom_));
      CurDom = std::move(CurDom_);
    }
    auto [LowerBound, UpperBound] = ranges::equal_range(CurDom, BasicBlockPtr);
    if (LowerBound == UpperBound) { CurDom.insert(UpperBound, BasicBlockPtr); }
    auto UpdateIter = Dominator->find(BasicBlockPtr);
    if (CurDom != std::get<1>(*UpdateIter)) {
      Changed = true;
      std::get<1>(*UpdateIter) = std::move(CurDom);
    }
  }
  return (Changed) ? PassStatus::InProgress : PassStatus::Converged;
}

void DominatorPass::finalize() {
  Function = nullptr;
  N = nullptr;
}

DominatorPass::AnalysisResult DominatorPass::getResult() const {
  return DominatorPassResult(Dominator);
}

bool DominatorPass::isSkipped(mir::Function const &Function_) const {
  return Function_.isDeclaration();
}

} // namespace mir::passes