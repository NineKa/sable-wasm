#include "SimplifyCFG.h"

#include <range/v3/view/filter.hpp>

namespace mir::passes {

bool SimplifyCFGPass::simplifyTrivialPhi(mir::BasicBlock &BasicBlock) {
  for (auto &Instruction : BasicBlock)
    if (is_a<instructions::Phi>(Instruction)) {
      auto &PhiNode = dyn_cast<instructions::Phi>(Instruction);
      if (PhiNode.getNumCandidates() == 1) {
        auto [Value, Path] = PhiNode.getCandidate(0);
        utility::ignore(Path);
        PhiNode.replaceAllUseWith(Value);
        PhiNode.eraseFromParent();
        return true;
      }
    }
  return false;
}

bool SimplifyCFGPass::simplifyTrivialBranch(mir::BasicBlock &BasicBlock) {
  auto InwardFlow = BasicBlock.getInwardFlow();
  if (InwardFlow.size() == 1) {
    auto &PreviousBlock = **InwardFlow.begin();
    auto OutwardFlow = PreviousBlock.getOutwardFlow();
    if (OutwardFlow.size() == 1) {
      assert(*OutwardFlow.begin() == std::addressof(BasicBlock));
      PreviousBlock.pop_back();
      PreviousBlock.splice(PreviousBlock.end(), BasicBlock);
      BasicBlock.replaceAllUseWith(std::addressof(PreviousBlock));
      BasicBlock.eraseFromParent();
      return true;
    }
  }
  return false;
}

bool SimplifyCFGPass::deadBasicBlockElem(mir::BasicBlock &BasicBlock) {
  if (!BasicBlock.isEntryBlock() && BasicBlock.hasNoInwardFlow()) {
    for (auto *SuccessorBB : BasicBlock.getOutwardFlow())
      for (auto &Instruction : *SuccessorBB) {
        if (!mir::is_a<mir::instructions::Phi>(Instruction)) continue;
        auto &PhiNode = mir::dyn_cast<mir::instructions::Phi>(Instruction);
        using VectorT =
            std::vector<std::pair<mir::Instruction *, mir::BasicBlock *>>;
        // clang-format off
        auto Candidates = PhiNode.getCandidates()
          | ranges::views::filter([&](auto const &CandidatePair) {
              return std::get<1>(CandidatePair) != std::addressof(BasicBlock);
            })
          | ranges::to<VectorT>();
        PhiNode.setCandidates(Candidates);
      }
    BasicBlock.eraseFromParent();
    return true;
  }
  return false;
}

namespace {
bool isDroppableInstruction(Instruction const &Inst) {
  switch (Inst.getInstructionKind()) {
  case InstructionKind::Select:
  case InstructionKind::LocalGet:
  case InstructionKind::GlobalGet:
  case InstructionKind::Constant:
  case InstructionKind::IntUnaryOp:
  case InstructionKind::IntBinaryOp:
  case InstructionKind::FPUnaryOp:
  case InstructionKind::FPBinaryOp:
  case InstructionKind::Load:
  case InstructionKind::MemorySize:
  case InstructionKind::Cast:
  case InstructionKind::Extend:
  case InstructionKind::Pack:
  case InstructionKind::Unpack:
  case InstructionKind::Phi: return true;
  default: return false;
  }
}
} // namespace

bool SimplifyCFGPass::deadInstructionElem(mir::BasicBlock &BasicBlock) {
  for (auto &Instruction : BasicBlock)
    if (Instruction.hasNoUsedSites() && isDroppableInstruction(Instruction)) {
      Instruction.eraseFromParent();
      return true;
    }
  return false;
}

void SimplifyCFGPass::prepare(mir::Function &Function_) {
  Function = std::addressof(Function_);
}

PassStatus SimplifyCFGPass::run() {
  for (auto &BasicBlock : Function->getBasicBlocks().asView()) {
    if (simplifyTrivialPhi(BasicBlock)) return PassStatus::InProgress;
    if (simplifyTrivialBranch(BasicBlock)) return PassStatus::InProgress;
    if (deadBasicBlockElem(BasicBlock)) return PassStatus::InProgress;
    if (deadInstructionElem(BasicBlock)) return PassStatus::InProgress;
  }
  return PassStatus::Converged;
}

void SimplifyCFGPass::finalize() {}

bool SimplifyCFGPass::isSkipped(mir::Function const &Function_) const {
  return Function_.isDeclaration();
}

void SimplifyCFGPass::getResult() const {}

} // namespace mir::passes