#include "Instruction.h"
#include "BasicBlock.h"

#include <range/v3/view/filter.hpp>

namespace mir {
bool Instruction::isPhi() const { return instructions::Phi::classof(this); }

bool Instruction::isBranching() const {
  switch (getInstructionKind()) {
  case InstructionKind::Branch:
  case InstructionKind::BranchTable: return true;
  default: return false;
  }
}

bool Instruction::isTerminating() const {
  switch (getInstructionKind()) {
  case InstructionKind::Unreachable:
  case InstructionKind::Branch:
  case InstructionKind::BranchTable:
  case InstructionKind::Return: return true;
  default: return false;
  }
}

bool Instruction::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::Instruction;
}

void Instruction::eraseFromParent() {
  auto &Parent = *getParent();
  Parent.erase(this);
}

void Instruction::replaceAllUseWith(Instruction *ReplaceValue) {
  // clang-format off
  auto ReplaceQueue = getUsedSites() 
    | ranges::view::filter([](auto const *Node) {
        return is_a<mir::Instruction>(Node);
      }) 
    | ranges::to<std::vector<ASTNode *>>();
  // clang-format on
  for (auto *Node : ReplaceQueue) {
    auto *CastedPtr = dyn_cast<Instruction>(Node);
    CastedPtr->replace(this, ReplaceValue);
  }
}
} // namespace mir

namespace fmt {
char const *formatter<mir::InstructionKind>::getEnumString(
    mir::InstructionKind const &Kind) {
  using IKind = mir::InstructionKind;
  // clang-format off
  switch (Kind) {
  case IKind::Unreachable : return "unreachable";
  case IKind::Branch      : return "br";
  case IKind::BranchTable : return "br.table";
  case IKind::Return      : return "ret";
  case IKind::Call        : return "call";
  case IKind::CallIndirect: return "call_indirect";
  case IKind::Select      : return "select";
  case IKind::LocalGet    : return "local.get";
  case IKind::LocalSet    : return "local.set";
  case IKind::GlobalGet   : return "global.get";
  case IKind::GlobalSet   : return "global.set";
  case IKind::Constant    : return "const";
  case IKind::IntUnaryOp  : return "int.unary";
  case IKind::IntBinaryOp : return "int.binary";
  case IKind::FPUnaryOp   : return "fp.unary";
  case IKind::FPBinaryOp  : return "fp.binary";
  case IKind::Load        : return "load";
  case IKind::Store       : return "store";
  case IKind::MemoryGuard : return "memory.guard";
  case IKind::MemoryGrow  : return "memory.grow";
  case IKind::MemorySize  : return "memory.size";
  case IKind::Cast        : return "cast";
  case IKind::Extend      : return "extend";
  case IKind::Pack        : return "pack";
  case IKind::Unpack      : return "unpack";
  case IKind::Phi         : return "phi";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt
