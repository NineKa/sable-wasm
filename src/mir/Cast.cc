#include "Cast.h"

namespace mir::instructions {
Cast::Cast(CastOpcode Opcode_, mir::Instruction *Operand_)
    : Instruction(InstructionKind::Cast), Opcode(Opcode_), Operand() {
  setOperand(Operand_);
}

Cast::~Cast() noexcept {
  if (Operand != nullptr) Operand->remove_use(this);
}

CastOpcode Cast::getCastOpcode() const { return Opcode; }
void Cast::setCastOpcode(CastOpcode Opcode_) { Opcode = Opcode_; }
mir::Instruction *Cast::getOperand() const { return Operand; }

void Cast::setOperand(mir::Instruction *Operand_) {
  if (Operand != nullptr) Operand->remove_use(this);
  if (Operand_ != nullptr) Operand_->add_use(this);
  Operand = Operand_;
}

void Cast::replace(ASTNode const *Old, ASTNode *New) noexcept {
  if (getOperand() == Old) setOperand(dyn_cast<Instruction>(New));
}

Type Cast::getCastToType() const {
  switch (getCastOpcode()) {
#define X(Name, Str, ResultTy, ParamTy)                                        \
  case CastOpcode::Name:                                                       \
    return Type::BuildPrimitive##ResultTy();
#include "Cast.defs"
#undef X
  default: utility::unreachable();
  }
}

Type Cast::getCastFromType() const {
  switch (getCastOpcode()) {
#define X(Name, Str, ResultTy, ParamTy)                                        \
  case CastOpcode::Name:                                                       \
    return Type::BuildPrimitive##ParamTy();
#include "Cast.defs"
#undef X
  default: utility::unreachable();
  }
}

bool Cast::classof(mir::Instruction const *Inst) {
  return Inst->getInstructionKind() == InstructionKind::Cast;
}

bool Cast::classof(mir::ASTNode const *Node) {
  if (Instruction::classof(Node))
    return Cast::classof(dyn_cast<Instruction>(Node));
  return false;
}
} // namespace mir::instructions

namespace fmt {
char const *formatter<mir::instructions::CastOpcode>::toString(
    mir::instructions::CastOpcode const &Opcode) {
  using CastOpcode = mir::instructions::CastOpcode;
  switch (Opcode) {
#define X(Name, Str, ...)                                                      \
  case CastOpcode::Name:                                                       \
    return Str;
#include "Cast.defs"
#undef X
  default: utility::unreachable();
  }
}
} // namespace fmt