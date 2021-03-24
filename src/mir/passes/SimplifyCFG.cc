#include "SimplifyCFG.h"

namespace mir::passes {

bool SimplifyCFGPass::simplifyTrivialPhi(mir::BasicBlock &BasicBlock) {
  std::vector<Instruction *> KillSet;
  for (auto &Instruction : BasicBlock) {
    if (!is_a<instructions::Phi>(Instruction)) continue;
    auto *CastedPtr = dyn_cast<instructions::Phi>(std::addressof(Instruction));
    if (CastedPtr->getNumCandidates() != 1) continue;
    auto [Value, Path] = CastedPtr->getCandidate(0);
    utility::ignore(Path);
    Instruction.replaceAllUseWith(Value);
    KillSet.push_back(CastedPtr);
  }
  for (auto *KillInst : KillSet) BasicBlock.erase(KillInst);
  return !KillSet.empty();
}

bool SimplifyCFGPass::simplifyTrivialBranch(mir::BasicBlock &BasicBlock) {
  auto &EnclosingFunction = *BasicBlock.getParent();
  auto *CurrentBlock = std::addressof(BasicBlock);
  auto InwardFlow = CurrentBlock->getInwardFlow();
  if (InwardFlow.size() != 1) return false;
  auto *PreviousBlock = *InwardFlow.begin();
  auto OutwardFlow = PreviousBlock->getOutwardFlow();
  if (OutwardFlow.size() != 1) return false;
  assert(*OutwardFlow.begin() == CurrentBlock);

  auto *PreviousBlockLastinst = std::addressof(PreviousBlock->back());
  PreviousBlock->erase(PreviousBlockLastinst);
  PreviousBlock->splice(PreviousBlock->end(), *CurrentBlock);
  EnclosingFunction.getBasicBlocks().erase(CurrentBlock);
  return true;
}

void SimplifyCFGPass::prepare(mir::Function &Function_) {
  Function = std::addressof(Function_);
}

PassStatus SimplifyCFGPass::run() {
  for (auto &BasicBlock : Function->getBasicBlocks().asView())
    if (simplifyTrivialPhi(BasicBlock)) return PassStatus::InProgress;
  for (auto &BasicBlock : Function->getBasicBlocks().asView())
    if (simplifyTrivialBranch(BasicBlock)) return PassStatus::InProgress;
  return PassStatus::Converged;
}

void SimplifyCFGPass::finalize() {}

bool SimplifyCFGPass::isSkipped(mir::Function const &Function_) const {
  return Function_.isDeclaration();
}

void SimplifyCFGPass::getResult() const {}

} // namespace mir::passes