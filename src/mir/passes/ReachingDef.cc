#include "ReachingDef.h"

namespace mir::passes {

ReachingDefPassResult::ReachingDefPassResult(
    std::shared_ptr<InMap> Ins_, std::shared_ptr<OutMap> Outs_)
    : Ins(std::move(Ins_)), Outs(std::move(Outs_)) {}

ReachingDefPassResult::DefSetView
ReachingDefPassResult::in(BasicBlock const &BB) const {
  auto SearchIter = Ins->find(std::addressof(BB));
  assert(SearchIter != Ins->end());
  return std::get<1>(*SearchIter);
}

ReachingDefPassResult::DefSetView
ReachingDefPassResult::out(BasicBlock const &BB) const {
  auto SearchIter = Outs->find(std::addressof(BB));
  assert(SearchIter != Outs->end());
  return std::get<1>(*SearchIter);
}

void ReachingDefPass::prepare(mir::Function &Function_) {
  assert(Function_.hasBody());
  Ins = std::make_shared<ReachingDefPassResult::InMap>();
  Outs = std::make_shared<ReachingDefPassResult::OutMap>();
  Function = std::addressof(Function_);
  using DefSet = ReachingDefPassResult::DefSet;
  for (auto &BasicBlock : Function_.getBasicBlocks()) {
    ReachingDefPassResult::DefSet Out;
    for (auto &Instruction : BasicBlock) 
      Out.insert(std::addressof(Instruction));
    Ins->emplace(std::addressof(BasicBlock), DefSet{});
    Outs->emplace(std::addressof(BasicBlock), std::move(Out));
  }
}

bool ReachingDefPass::run(mir::BasicBlock *BasicBlock) {
  auto Changed = false;
  auto& In = getIn(*BasicBlock);
  auto& Out = getOut(*BasicBlock);
  for (auto const *Predecessor : BasicBlock->getInwardFlow()) {
    auto const &PredecessorOut = getOut(*Predecessor);
    for (auto *Def : PredecessorOut) {
      if (In.find(Def) != In.end()) continue;
      In.insert(Def);
      Out.insert(Def);
      Changed = true;
    }
  }
  return Changed;
}

ReachingDefPassResult::DefSet &ReachingDefPass::getIn(BasicBlock const &BB) {
  auto SearchIter = Ins->find(std::addressof(BB));
  assert(SearchIter != Ins->end());
  return std::get<1>(*SearchIter);
}

ReachingDefPassResult::DefSet &ReachingDefPass::getOut(BasicBlock const &BB) {
  auto SearchIter = Outs->find(std::addressof(BB));
  assert(SearchIter != Outs->end());
  return std::get<1>(*SearchIter);
}

PassStatus ReachingDefPass::run() {
  auto Result = PassStatus::Converged;
  for (auto &BasicBlock : Function->getBasicBlocks()) {
    if (run(std::addressof(BasicBlock))) { Result = PassStatus::InProgress; }
  }
  return Result;
}

void ReachingDefPass::finalize() { Function = nullptr; }

ReachingDefPass::AnalysisResult ReachingDefPass::getResult() const {
  return ReachingDefPassResult(Ins, Outs);
}

} // namespace mir::passes