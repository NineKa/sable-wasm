#ifndef SABLE_INCLUDE_GUARD_MIR_INTRINSIC
#define SABLE_INCLUDE_GUARD_MIR_INTRINSIC

#include "Instruction.h"

namespace mir {

enum class IntrinsicKind {};
class Intrinsic : public Instruction {
  IntrinsicKind Kind;

public:
  InstructionKind getIntrinsicKind() const;
  virtual Type getResultType() const = 0;
};

} // namespace mir

#endif