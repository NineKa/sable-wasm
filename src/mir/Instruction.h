#ifndef SABLE_INCLUDE_GUARD_MIR_IR
#define SABLE_INCLUDE_GUARD_MIR_IR

#include "../bytecode/Type.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <range/v3/core.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/single.hpp>
#include <range/v3/view/subrange.hpp>

#include <forward_list>
#include <variant>

namespace mir {
class BasicBlock;

enum class InstructionKind : std::uint8_t {
  Unreachable,
  Branch,
  Constant,
  IntUnaryOp,
  IntBinaryOp,
  FPUnaryOp,
  FPBinaryOp
};

class Instruction
    : public llvm::ilist_node_with_parent<Instruction, BasicBlock> {
  InstructionKind Kind;
  BasicBlock *Parent = nullptr;
  std::forward_list<Instruction *> UsedSites;

  friend class llvm::ilist_node_with_parent<Instruction, BasicBlock>;
  friend class llvm::ilist_alloc_traits<Instruction>;
  BasicBlock *getParent() const { return Parent; }

protected:
  explicit Instruction(InstructionKind Kind_, BasicBlock *BB)
      : Kind(Kind_), Parent(BB) {}

public:
  Instruction(Instruction const &) = delete;
  Instruction(Instruction &&) noexcept = delete;
  Instruction &operator=(Instruction const &) = delete;
  Instruction &operator=(Instruction &&) noexcept = delete;
  virtual ~Instruction() noexcept = 0;

  void addUsedSite(Instruction *Inst) { UsedSites.push_front(Inst); }
  void removeUsedSite(Instruction *Inst) { std::erase(UsedSites, Inst); }

  InstructionKind getKind() const { return Kind; }
  auto getUsedSites() const { return ranges::views::all(UsedSites); }
  BasicBlock *getEnclosingBasicBlock() const { return Parent; }

  using operand_iterator = std::add_pointer<Instruction *>::type;
  operand_iterator operand_begin();
  operand_iterator operand_end();
  auto getOperands() {
    return ranges::subrange(operand_begin(), operand_end()) |
           ranges::views::filter(
               [](Instruction *Inst) { return Inst != nullptr; });
  }

  using bb_iterator = std::add_pointer<BasicBlock *>::type;
  bb_iterator basic_block_begin();
  bb_iterator basic_block_end();
};

class BasicBlock {
  llvm::ilist<Instruction> Instructions;
  // keep track which branch instruction can reach to this basic block
  std::forward_list<Instruction *> UsedSites;

public:
  BasicBlock() = default;
  BasicBlock(BasicBlock const &) = delete;
  BasicBlock(BasicBlock &&) noexcept = delete;
  BasicBlock &operator=(BasicBlock const &) = delete;
  BasicBlock &operator=(BasicBlock &&) noexcept = delete;
  ~BasicBlock() noexcept;
  static llvm::ilist<Instruction> BasicBlock::*getSublistAccess(Instruction *) {
    return &BasicBlock::Instructions;
  }

  using iterator = decltype(Instructions)::iterator;
  iterator begin() { return Instructions.begin(); }
  iterator end() { return Instructions.end(); }

  void addUsedSite(Instruction *Inst) { UsedSites.push_front(Inst); }
  void removeUsedSite(Instruction *Inst) { std::erase(UsedSites, Inst); }

  template <std::derived_from<Instruction> T, typename... ArgTypes>
  Instruction *append(ArgTypes &&...Args) {
    auto *Inst = new T(this, std::forward<ArgTypes>(Args)...); // NOLINT
    Instructions.push_back(Inst);
    return Inst;
  }
};
} // namespace mir

namespace mir::instructions {
///////////////////////////////// Unreachable //////////////////////////////////
class Unreachable : public Instruction {
public:
  Unreachable(BasicBlock *Parent);
  operand_iterator operand_begin() { return nullptr; }
  operand_iterator operand_end() { return nullptr; }
  static bool classof(Instruction const *Inst) {
    return Inst->getKind() == InstructionKind::Unreachable;
  }
};

//////////////////////////////////// Branch ////////////////////////////////////
class Branch : public Instruction {
  std::array<BasicBlock *, 1> Target;
  std::array<Instruction *, 1> Operands;

public:
  Branch(BasicBlock *Parent, BasicBlock *Target_, Instruction *Condition_);
  Branch(BasicBlock *Parent, BasicBlock *Target_);
  bool isConditional() const;
  bool isUnconditional() const;
  Instruction *getCondition() const;
  void setCondition(Instruction *);
  BasicBlock *getTarget() const;
  void setTarget(BasicBlock *Target_);
  operand_iterator operand_begin() { return Operands.begin(); }
  operand_iterator operand_end() { return Operands.end(); }
  bb_iterator basic_block_begin() { return Target.begin(); }
  bb_iterator basic_block_end() { return Target.end(); }
  static bool classof(Instruction const *Inst) {
    return Inst->getKind() == InstructionKind::Branch;
  }
};

////////////////////////////////// Constant ////////////////////////////////////
class Constant : public Instruction {
  std::variant<std::int32_t, std::int64_t, float, double> Storage;

public:
  Constant(BasicBlock *Parent, std::int32_t Value);
  Constant(BasicBlock *Parent, std::int64_t Value);
  Constant(BasicBlock *Parent, float Value);
  Constant(BasicBlock *Parent, double Value);
  std::int32_t getI32() const { return std::get<std::int32_t>(Storage); }
  void setI32(std::int32_t Value) { Storage = Value; }
  std::int64_t getI64() const { return std::get<std::int64_t>(Storage); }
  void setI64(std::int64_t Value) { Storage = Value; }
  float getF32() const { return std::get<float>(Storage); }
  void setF32(float Value) { Storage = Value; }
  double getF64() const { return std::get<double>(Storage); }
  void setF64(double Value) { Storage = Value; }
  operand_iterator operand_begin() { return nullptr; }
  operand_iterator operand_end() { return nullptr; }
  static bool classof(Instruction const *Inst) {
    return Inst->getKind() == InstructionKind::Constant;
  }
};

////////////////////////////////// IntUnaryOp //////////////////////////////////
enum class IntUnaryOperator : std::uint8_t { Eqz, Clz, Ctz, Popcnt };
class IntUnaryOp : public Instruction {
  IntUnaryOperator Operator;
  std::array<Instruction *, 1> Operands;

public:
  IntUnaryOp(
      BasicBlock *Parent, IntUnaryOperator Operator_, Instruction *Operand);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand);
  IntUnaryOperator getOperator() const;
  operand_iterator operand_begin() { return Operands.begin(); }
  operand_iterator operand_end() { return Operands.end(); }
  static bool classof(Instruction const *Inst) {
    return Inst->getKind() == InstructionKind::IntUnaryOp;
  }
};

///////////////////////////////// IntBinaryOp //////////////////////////////////
// clang-format off
enum class IntBinaryOperator: std::uint8_t {
  Eq, Ne, LtS, LtU, GtS, GtU, LeS, LeU, GeS, GeU,
  Add, Sub, Mul, DivS, DivU, RemS, RemU, And, Or, Xor,
  Shl, ShrS, ShrU, Rotl, Rotr
};
// clang-format on
class IntBinaryOp : public Instruction {
  IntBinaryOperator Operator;
  std::array<Instruction *, 2> Operands;

public:
  IntBinaryOp(
      BasicBlock *Parent, IntBinaryOperator Operator_, Instruction *LHS_,
      Instruction *RHS_);
  Instruction *getLHS() const;
  void setLHS(Instruction *Operand);
  Instruction *getRHS() const;
  void setRHS(Instruction *Operand);
  IntBinaryOperator getOperator() const;
  bool isComparison() const;
  bool isMulOrDiv() const;
  operand_iterator operand_begin() { return Operands.begin(); }
  operand_iterator operand_end() { return Operands.end(); }
  static bool classof(Instruction const *Inst) {
    return Inst->getKind() == InstructionKind::IntBinaryOp;
  }
};

////////////////////////////////// FPUnaryOp ///////////////////////////////////
// clang-format off
enum class FPUnaryOperator : std::uint8_t {
  Abs, Neg, Ceil, Floor, Trunc, Nearest, Sqrt
};
// clang-format on
class FPUnaryOp : public Instruction {
  FPUnaryOperator Operator;
  std::array<Instruction *, 1> Operands;

public:
  FPUnaryOp(
      BasicBlock *Parent, FPUnaryOperator Operator_, Instruction *Operand_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand);
  FPUnaryOperator getOperator() const;
  operand_iterator operand_begin() { return Operands.begin(); }
  operand_iterator operand_end() { return Operands.end(); }
  static bool classof(Instruction const *Inst) {
    return Inst->getKind() == InstructionKind::FPUnaryOp;
  }
};

///////////////////////////////// FPBinaryOp ///////////////////////////////////
// clang-format off
enum class FPBinaryOperator : std::uint8_t {
  Eq, Ne, Lt, Gt, Le, Ge, Add, Sub, Mul, Div, Min, Max, CopySign
};
// clang-format on
class FPBinaryOp : public Instruction {
  FPBinaryOperator Operator;
  std::array<Instruction *, 2> Operands;

public:
  FPBinaryOp(
      BasicBlock *Parent, FPBinaryOperator Operator_, Instruction *LHS_,
      Instruction *RHS_);
  Instruction *getLHS() const;
  void setLHS(Instruction *Operand);
  Instruction *getRHS() const;
  void setRHS(Instruction *Operand);
  FPBinaryOperator getOperator() const;
  bool isComparison() const;
  operand_iterator operand_begin() { return Operands.begin(); }
  operand_iterator operand_end() { return Operands.end(); }
  static bool classof(Instruction const *Inst) {
    return Inst->getKind() == InstructionKind::FPBinaryOp;
  }
};

} // namespace mir::instructions

namespace mir {
template <typename T>
concept instruction = std::derived_from<T, Instruction> &&requires(T Inst) {
  { T::classof(std::declval<Instruction const *>()) }
  ->std::convertible_to<bool>;
};

template <typename Derived, typename RetType = void, bool Const = true>
class InstVisitor {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <instruction T> RetType castAndCall(Ptr<Instruction> Inst) {
    return derived()(static_cast<Ptr<T>>(Inst));
  }

public:
  RetType visit(Ptr<Instruction> Inst) {
    using namespace mir::instructions;
    using IKind = InstructionKind;
    switch (Inst->getKind()) {
    case IKind::Unreachable: return castAndCall<Unreachable>(Inst);
    case IKind::Branch: return castAndCall<Branch>(Inst);
    case IKind::Constant: return castAndCall<Constant>(Inst);
    case IKind::IntUnaryOp: return castAndCall<IntUnaryOp>(Inst);
    case IKind::IntBinaryOp: return castAndCall<IntBinaryOp>(Inst);
    case IKind::FPUnaryOp: return castAndCall<FPUnaryOp>(Inst);
    case IKind::FPBinaryOp: return castAndCall<FPBinaryOp>(Inst);
    default: SABLE_UNREACHABLE();
    }
  }
};
} // namespace mir

#endif
