#include "BasicBlock.h"
#include "Instruction.h"

namespace mir {
BasicBlock::BasicBlock(Function *Parent_)
    : ASTNode(ASTNodeKind::BasicBlock), Parent(Parent_) {}

bool BasicBlock::empty() const { return Instructions.empty(); }

std::vector<BasicBlock *> BasicBlock::getInwardFlow() const {
  std::vector<BasicBlock *> Result;
  for (auto const *ASTNode : this->getUsedSites()) {
    if (!is_a<Instruction>(ASTNode)) continue;
    auto *CastedPtr = dyn_cast<Instruction>(ASTNode);
    switch (CastedPtr->getInstructionKind()) {
    case InstructionKind::Branch:
    case InstructionKind::BranchTable:
      Result.push_back(CastedPtr->getParent());
      break;
    default: break;
    }
  }
  return Result;
}

std::vector<BasicBlock *> BasicBlock::getOutwardFlow() const {
  std::vector<BasicBlock *> Result;
  if (!empty()) {
    auto *TerminatingInst = std::addressof(Instructions.back());
    switch (TerminatingInst->getInstructionKind()) {
    case InstructionKind::Branch: {
      auto *CastedPtr = dyn_cast<instructions::Branch>(TerminatingInst);
      auto *Target = CastedPtr->getTarget();
      if (Target != nullptr) Result.push_back(CastedPtr->getTarget());
      if (CastedPtr->isConditional()) {
        auto *FalseTarget = CastedPtr->getFalseTarget();
        if (FalseTarget != nullptr)
          Result.push_back(CastedPtr->getFalseTarget());
      }
      break;
    }
    case InstructionKind::BranchTable: {
      auto *CastedPtr = dyn_cast<instructions::BranchTable>(TerminatingInst);
      auto *DefaultTarget = CastedPtr->getDefaultTarget();
      if (DefaultTarget != nullptr) Result.push_back(DefaultTarget);
      for (auto *Target : CastedPtr->getTargets())
        if (Target != nullptr) Result.push_back(Target);
      break;
    }
    default: break;
    }
  }
  return Result;
}

llvm::ilist<Instruction> BasicBlock::*
BasicBlock::getSublistAccess(Instruction *) {
  return &BasicBlock::Instructions;
}

void BasicBlock::detach(ASTNode const *) noexcept { utility::unreachable(); }
bool BasicBlock::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::BasicBlock;
}
} // namespace mir