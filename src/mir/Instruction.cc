#include "Instruction.h"

#include "../utility/Commons.h"

namespace mir {} // namespace mir

namespace llvm {
void ilist_alloc_traits<mir::Instruction>::deleteNode(mir::Instruction *Inst) {
  Inst->Parent = nullptr;
  for (auto *UsedSite : Inst->getUsedSites()) UsedSite->detachOperand(Inst);
  delete Inst;
}
} // namespace llvm

namespace mir::instructions {
using IKind = InstructionKind;
///////////////////////////////// Unreachable //////////////////////////////////
Unreachable::Unreachable(BasicBlock *Parent)
    : Instruction(IKind::Unreachable, Parent) {}
void Unreachable::detachOperand(Instruction *) {
  assert(false); // unreachable instruction will never have a operand
}
MIRType Unreachable::getType() const { return MIRType({}); }

////////////////////////////////// Constant ////////////////////////////////////
Constant::Constant(BasicBlock *Parent, std::int32_t Value)
    : Instruction(IKind::Constant, Parent), Storage(Value) {}
Constant::Constant(BasicBlock *Parent, std::int64_t Value)
    : Instruction(IKind::Constant, Parent), Storage(Value) {}
Constant::Constant(BasicBlock *Parent, float Value)
    : Instruction(IKind::Constant, Parent), Storage(Value) {}
Constant::Constant(BasicBlock *Parent, double Value)
    : Instruction(IKind::Constant, Parent), Storage(Value) {}
void Constant::detachOperand(Instruction *) {
  assert(false); // constant instruction will never have a operand
}
MIRType Constant::getType() const {
  using namespace bytecode::valuetypes;
  auto const Visitor = utility::Overload{
      [](std::int32_t) { return MIRType({I32}); },
      [](std::int64_t) { return MIRType({I64}); },
      [](float) { return MIRType({F32}); },
      [](double) { return MIRType({F64}); }};
  return std::visit(Visitor, Storage);
}
} // namespace mir::instructions
