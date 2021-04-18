#ifndef SABLE_INCLUDE_GUARD_MIR_CAST
#define SABLE_INCLUDE_GUARD_MIR_CAST

#include "Instruction.h"

#include <fmt/format.h>

namespace mir::instructions {

enum class CastOpcode {
#define X(Name, ...) Name,
#include "Cast.defs"
#undef X
};

class Cast : public Instruction {
  CastOpcode Opcode;
  mir::Instruction *Operand;

public:
  Cast(CastOpcode Opcode_, mir::Instruction *Operand_);
  Cast(Cast const &) = delete;
  Cast(Cast &&) noexcept = delete;
  Cast &operator=(Cast const &) = delete;
  Cast &operator=(Cast &&) noexcept = delete;
  ~Cast() noexcept override;

  CastOpcode getCastOpcode() const;
  void setCastOpcode(CastOpcode Opcode_);
  mir::Instruction *getOperand() const;
  void setOperand(mir::Instruction *Operand_);

  Type getCastToType() const;
  Type getCastFromType() const;

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(mir::Instruction const *Inst);
  static bool classof(mir::ASTNode const *Node);
};
} // namespace mir::instructions

namespace fmt {
template <> struct formatter<mir::instructions::CastOpcode> {
  template <typename CTX> auto parse(CTX &&C) { return C.begin(); }
  char const *toString(mir::instructions::CastOpcode const &Opcode);
  template <typename CTX>
  auto format(mir::instructions::CastOpcode const &Opcode, CTX &&C) {
    return fmt::format_to(C.out(), toString(Opcode));
  }
};
} // namespace fmt
#endif