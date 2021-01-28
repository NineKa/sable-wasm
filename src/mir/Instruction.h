#ifndef SABLE_INCLUDE_GUARD_MIR_IR
#define SABLE_INCLUDE_GUARD_MIR_IR

#include "../bytecode/Type.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <range/v3/core.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/single.hpp>

#include <forward_list>
#include <variant>

namespace mir {
class BasicBlock;
enum class InstructionKind : std::uint8_t { Unreachable, Constant, IBinaryOp };

// Type deduction result for MIR instructions. Unlike LLVM, we do not attach
// type information to each node. The type is inferred as requested via
// Instruction::getType()
class MIRType {
  bool UnderTypeError;
  llvm::SmallVector<bytecode::ValueType, 1> Types;

public:
  MIRType() : UnderTypeError(true) {}
  template <ranges::input_range T>
  MIRType(T const &Types_)
      : UnderTypeError(false), Types(ranges::to<decltype(Types)>(Types_)) {}
  MIRType(std::initializer_list<bytecode::ValueType> Types_)
      : UnderTypeError(false), Types(Types_) {}

  bool isErroneous() const { return UnderTypeError; }
  bool isVoid() const { return Types.size() == 0; }
  bool isValueType() const { return Types.size() == 1; }
  bool isPacked() const { return Types.size() > 1; }
  bytecode::ValueType get() const {
    assert(isValueType());
    return Types[0];
  }
  auto getPacked() const {
    assert(isPacked());
    return ranges::views::all(Types);
  }
};

class Instruction
    : public llvm::ilist_node_with_parent<Instruction, BasicBlock> {
  InstructionKind Kind;
  BasicBlock *Parent = nullptr;
  std::forward_list<Instruction *> UsedSites;

  friend class llvm::ilist_node_with_parent<Instruction, BasicBlock>;
  BasicBlock *getParent() const { return Parent; }
  // called when one of the operand is detached from the AST
  friend struct llvm::ilist_alloc_traits<Instruction>;
  virtual void detachOperand(Instruction *) = 0;

protected:
  explicit Instruction(InstructionKind Kind_, BasicBlock *BB)
      : Kind(Kind_), Parent(BB) {}

public:
  Instruction(Instruction const &) = default;
  Instruction(Instruction &&) noexcept = default;
  Instruction &operator=(Instruction const &) = delete;
  Instruction &operator=(Instruction &&) noexcept = delete;
  virtual ~Instruction() noexcept = default;

  virtual MIRType getType() const = 0;

  InstructionKind getKind() const { return Kind; }
  auto getUsedSites() const { return ranges::views::all(UsedSites); }
  BasicBlock *getEnclosingBasicBlock() const { return Parent; }
};
} // namespace mir

namespace llvm {
template <> struct ilist_alloc_traits<mir::Instruction> {
  static void deleteNode(mir::Instruction *);
};
} // namespace llvm

namespace fmt {
template <> struct formatter<mir::MIRType> {
  template <typename C> auto parse(C &&CTX) const { return CTX.begin(); }
  template <typename C> auto format(mir::MIRType const &Type, C &&CTX) const {
    if (Type.isVoid()) { return fmt::format_to(CTX.out(), "void"); }
    if (Type.isValueType()) {
      return fmt::format_to(CTX.out(), "{}", Type.get());
    }
    if (Type.isPacked()) {
      char const *Separator = "";
      auto Out = fmt::format_to(CTX.out(), "[");
      for (auto const &ValueType : Type.getPacked()) {
        Out = fmt::format_to(Out, "{}{}", Separator, ValueType);
        Separator = ", ";
      }
      return fmt::format_to(CTX.out(), "]");
    }
    SABLE_UNREACHABLE();
  }
};
} // namespace fmt

namespace mir::instructions {
///////////////////////////////// Unreachable //////////////////////////////////
class Unreachable : public Instruction {
  void detachOperand(Instruction *) override;

public:
  Unreachable(BasicBlock *Parent);
  MIRType getType() const override;
  static bool classof(Instruction *Inst) {
    return Inst->getKind() == InstructionKind::Unreachable;
  }
};

////////////////////////////////// Constant ////////////////////////////////////
class Constant : public Instruction {
  std::variant<std::int32_t, std::int64_t, float, double> Storage;
  void detachOperand(Instruction *) override;

public:
  explicit Constant(BasicBlock *Parent, std::int32_t Value);
  explicit Constant(BasicBlock *Parent, std::int64_t Value);
  explicit Constant(BasicBlock *Parent, float Value);
  explicit Constant(BasicBlock *Parent, double Value);
  std::int32_t getI32() const { return std::get<std::int32_t>(Storage); }
  void setI32(std::int32_t Value) { Storage = Value; }
  std::int64_t getI64() const { return std::get<std::int64_t>(Storage); }
  void setI64(std::int64_t Value) { Storage = Value; }
  float getF32() const { return std::get<float>(Storage); }
  void setF32(float Value) { Storage = Value; }
  double getF64() const { return std::get<double>(Storage); }
  void setF64(double Value) { Storage = Value; }
  MIRType getType() const override;
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

} // namespace mir::instructions

#endif
