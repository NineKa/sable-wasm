#ifndef SABLE_INCLUDE_GUARD_MIR_IR
#define SABLE_INCLUDE_GUARD_MIR_IR

#include "../bytecode/Type.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/ilist_node.h>
#include <range/v3/core.hpp>
#include <range/v3/view/all.hpp>

#include <variant>

namespace mir {
class BasicBlock;
enum class InstructionKind : std::uint8_t { Unreachable, Constant, IBinaryOp };

using InstResultType = llvm::SmallVector<bytecode::ValueType, 4>;

class Instruction
    : public llvm::ilist_node_with_parent<Instruction, BasicBlock> {
  BasicBlock *Parent = nullptr;
  InstructionKind Kind;
  InstResultType Type;

  friend class llvm::ilist_node_with_parent<Instruction, BasicBlock>;
  BasicBlock *getParent() const { return Parent; }

public:
  explicit Instruction(InstructionKind Kind_, InstResultType Type_)
      : Kind(Kind_), Type(std::move(Type_)) {}
  Instruction(Instruction const &) = default;
  Instruction(Instruction &&) noexcept = default;
  Instruction &operator=(Instruction const &) = delete;
  Instruction &operator=(Instruction &&) noexcept = delete;
  virtual ~Instruction() noexcept = default;

  InstructionKind getKind() const { return Kind; }
  auto getResultType() const { return ranges::views::all(Type); }
};

namespace instructions {
///////////////////////////////// Unreachable //////////////////////////////////
class Unreachable : public Instruction {
public:
  Unreachable();

  static bool classof(Instruction *Inst) {
    return Inst->getKind() == InstructionKind::Unreachable;
  }
};

////////////////////////////////// Constant ////////////////////////////////////
class Constant : public Instruction {
  std::variant<std::int32_t, std::int64_t, float, double> Storage;

public:
  explicit Constant(std::int32_t Value);
  explicit Constant(std::int64_t Value);
  explicit Constant(float Value);
  explicit Constant(double Value);
  std::int32_t asI32() const { return std::get<std::int32_t>(Storage); }
  std::int64_t asI64() const { return std::get<std::int64_t>(Storage); }
  float asF32() const { return std::get<float>(Storage); }
  double asF64() const { return std::get<double>(Storage); }
  static bool classof(Instruction *Inst) {
    return Inst->getKind() == InstructionKind::Constant;
  }
};

////////////////////////////////// IBinaryOp ///////////////////////////////////
// clang-format off
enum class IBinaryOpKind : std::uint8_t {
  Add, Sub, Mul, Div, Rem, And, Or , Xor, Shl, Shr, Rotr, Rotl
};
// clang-format on
class IBinaryOp : public Instruction {
  IBinaryOpKind Kind;
  bool IsSigned;
  Instruction *LHS;
  Instruction *RHS;

public:
  IBinaryOp(IBinaryOpKind, bool, Instruction *, Instruction *);
  IBinaryOp(IBinaryOpKind, Instruction *, Instruction *);
  IBinaryOpKind getOpKind() const { return Kind; }
  bool isSignedOp() const { return IsSigned; }
  bool needSignedness() const;
  Instruction *getLHS() const { return LHS; }
  Instruction *getRHS() const { return RHS; }
  static bool classof(Instruction *Inst) {
    return Inst->getKind() == InstructionKind::IBinaryOp;
  }
};

} // namespace instructions

} // namespace mir

#endif
