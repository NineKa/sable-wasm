#include "Instruction.h"

#include "../utility/Commons.h"

using namespace bytecode::valuetypes;

namespace mir::instructions {
namespace {
InstResultType Void() { return {}; }
} // namespace

///////////////////////////////// Unreachable //////////////////////////////////
Unreachable::Unreachable()
    : Instruction(InstructionKind::Unreachable, Void()) {}

////////////////////////////////// Constant ////////////////////////////////////
Constant::Constant(std::int32_t Value)
    : Instruction(InstructionKind::Constant, {I32}), Storage(Value) {}
Constant::Constant(std::int64_t Value)
    : Instruction(InstructionKind::Constant, {I64}), Storage(Value) {}
Constant::Constant(float Value)
    : Instruction(InstructionKind::Constant, {F32}), Storage(Value) {}
Constant::Constant(double Value)
    : Instruction(InstructionKind::Constant, {F64}), Storage(Value) {}

////////////////////////////////// IBinaryOp ///////////////////////////////////
namespace detail {
bool requireSignedness(IBinaryOpKind Kind) {
  switch (Kind) {
  case IBinaryOpKind::Add:
  case IBinaryOpKind::Sub:
  case IBinaryOpKind::Mul:
  case IBinaryOpKind::And:
  case IBinaryOpKind::Or:
  case IBinaryOpKind::Xor:
  case IBinaryOpKind::Shl:
  case IBinaryOpKind::Rotr:
  case IBinaryOpKind::Rotl: return false;
  case IBinaryOpKind::Shr:
  case IBinaryOpKind::Div:
  case IBinaryOpKind::Rem: return true;
  default: SABLE_UNREACHABLE();
  }
}

InstResultType computeIBinaryOpType(Instruction *LHS, Instruction *RHS) {
  assert(ranges::size(LHS->getResultType()) == 1);
  assert(ranges::size(RHS->getResultType()) == 1);
  auto LHSType = ranges::front(LHS->getResultType());
  auto RHSType = ranges::front(RHS->getResultType());
  if ((LHSType == I32) && (RHSType == I32)) return {I32};
  if ((LHSType == I64) && (RHSType == I64)) return {I64};
  SABLE_UNREACHABLE();
}
} // namespace detail

IBinaryOp::IBinaryOp(
    IBinaryOpKind Kind_, bool IsSigned_, Instruction *LHS_, Instruction *RHS_)
    : Instruction(
          InstructionKind::IBinaryOp, detail::computeIBinaryOpType(LHS_, RHS_)),
      Kind(Kind_), IsSigned(IsSigned_), LHS(LHS_), RHS(RHS_) {
  assert(detail::requireSignedness(Kind));
}

IBinaryOp::IBinaryOp(IBinaryOpKind Kind_, Instruction *LHS_, Instruction *RHS_)
    : Instruction(
          InstructionKind::IBinaryOp, detail::computeIBinaryOpType(LHS_, RHS_)),
      Kind(Kind_), IsSigned(false), LHS(LHS_), RHS(RHS_) {
  assert(!detail::requireSignedness(Kind));
}

bool IBinaryOp::needSignedness() const {
  return detail::requireSignedness(Kind);
}

} // namespace mir::instructions