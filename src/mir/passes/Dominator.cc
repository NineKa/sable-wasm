#include "Dominator.h"

#include <range/v3/algorithm/equal_range.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>

#include <iterator>

namespace mir::passes {

DominatorPassResult::DominatorPassResult(
    std::shared_ptr<DominatorMap> Dominator_)
    : Dominator(std::move(Dominator_)) {}

std::span<mir::BasicBlock *const>
DominatorPassResult::get(BasicBlock const &BB) const {
  auto SearchIter = Dominator->find(std::addressof(BB));
  assert(SearchIter != Dominator->end());
  return std::get<1>(*SearchIter);
}

bool DominatorPassResult::dominate(
    mir::BasicBlock const &V, mir::BasicBlock const &U) const {
  auto Dom = get(U);
  return ranges::find(Dom, std::addressof(V)) != Dom.end();
}

void DominatorPass::prepare(mir::Function &Function_) {
  using BasicBlockSet = DominatorPassResult::BasicBlockSet;
  using DominatorMap = DominatorPassResult::DominatorMap;
  assert(Function_.hasBody());
  Function = std::addressof(Function_);
  Dominator = std::make_shared<DominatorMap>();
  N = std::make_unique<BasicBlockSet>();
  N->reserve(Function->getNumBasicBlock());

  for (auto &BasicBlock : Function->getBasicBlocks())
    N->push_back(std::addressof(BasicBlock));
  ranges::sort(*N);

  for (auto &BasicBlock : Function->getBasicBlocks()) {
    auto *BasicBlockPtr = std::addressof(BasicBlock);
    if (BasicBlockPtr == Function->getEntryBasicBlock()) {
      Dominator->emplace(BasicBlockPtr, BasicBlockSet{BasicBlockPtr});
    } else {
      Dominator->emplace(BasicBlockPtr, *N);
    }
  }
}

PassStatus DominatorPass::run() {
  bool Changed = false;
  for (auto &BasicBlock : Function->getBasicBlocks()) {
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

} // namespace mir::passes