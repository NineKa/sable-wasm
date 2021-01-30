#ifndef SABLE_INCLUDE_GUARD_MIR_IR
#define SABLE_INCLUDE_GUARD_MIR_IR

#include "../bytecode/Type.h"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>

#include <forward_list>
#include <variant>

namespace mir {
class Module;

class Function;
class Global;
class Table;
class Memory;

class BasicBlock;
class Local;

namespace detail {
template <typename Def, typename Use> class UseSiteTraceable {
  std::forward_list<Use *> Uses;

public:
  UseSiteTraceable() = default;
  UseSiteTraceable(UseSiteTraceable const &) = delete;
  UseSiteTraceable(UseSiteTraceable &&) noexcept = delete;
  UseSiteTraceable &operator=(UseSiteTraceable const &) = delete;
  UseSiteTraceable &operator=(UseSiteTraceable &&) noexcept = delete;
  void add_use(Use *Referrer) { Uses.push_front(Referrer); }
  void remove_use(Use *Referrer) { std::erase(Uses, Referrer); }
  ~UseSiteTraceable() noexcept {
    for (auto *U : Uses) U->detach_definition(static_cast<Def *>(this));
  }

  using iterator = typename decltype(Uses)::iterator;
  iterator use_site_begin() { return Uses.begin(); }
  iterator use_site_end() { return Uses.end(); }
};
} // namespace detail

enum class InstructionKind : std::uint8_t {
  Unreachable,
  Branch,
  BranchTable,
  Return,
  Call,
  CallIndirect,
  Select,
  LocalGet,
  LocalSet,
  GlobalGet,
  GlobalSet,
  Constant,
  IntUnaryOp,
  IntBinaryOp,
  FPUnaryOp,
  FPBinaryOp
};

class Instruction :
    public llvm::ilist_node_with_parent<Instruction, BasicBlock>,
    public detail::UseSiteTraceable<Instruction, Instruction> {
  InstructionKind Kind;
  BasicBlock *Parent = nullptr;

protected:
  explicit Instruction(InstructionKind Kind_, BasicBlock *BB)
      : Kind(Kind_), Parent(BB) {}

public:
  Instruction(Instruction const &) = delete;
  Instruction(Instruction &&) noexcept = delete;
  Instruction &operator=(Instruction const &) = delete;
  Instruction &operator=(Instruction &&) noexcept = delete;
  virtual ~Instruction() noexcept = default;
  BasicBlock *getParent() const { return Parent; }
  InstructionKind getKind() const { return Kind; }

  // called when defined entity is removed from IR
  // a proper instruction should also remove them from operands (set to nullptr)
  // defaults to do nothing
  // called from destructors, must be noexcept
  // TODO: move these to traits
  virtual void detach_definition(Instruction *) noexcept {}
  virtual void detach_definition(Local *) noexcept {}
  virtual void detach_definition(BasicBlock *) noexcept {}
  virtual void detach_definition(Function *) noexcept {}
  virtual void detach_definition(Global *) noexcept {}
  virtual void detach_definition(Memory *) noexcept {}
  virtual void detach_definition(Table *) noexcept {}
};

class BasicBlock :
    public llvm::ilist_node_with_parent<BasicBlock, Function>,
    public detail::UseSiteTraceable<BasicBlock, Instruction> {
  Function *Parent;
  llvm::ilist<Instruction> Instructions;

public:
  // explicit BasicBlock(Function *Parent);
  auto getUsedSites() {
    return ranges::subrange(use_site_begin(), use_site_end());
  }

  template <std::derived_from<Instruction> T, typename... ArgTypes>
  Instruction *append(ArgTypes &&...Args) {
    auto *Inst = new T(this, std::forward<ArgTypes>(Args)...); // NOLINT
    Instructions.push_back(Inst);
    return Inst;
  }

  Function *getParent() const { return Parent; }
};

class Local :
    public llvm::ilist_node_with_parent<Local, Function>,
    public detail::UseSiteTraceable<Local, Instruction> {
  Function *Parent;
  bytecode::ValueType Type;

public:
  explicit Local(Function *Parent_, bytecode::ValueType Type_);
  Function *getParent() const { return Parent; }
  bytecode::ValueType const &getType() { return Type; }
};

class Function :
    public llvm::ilist_node_with_parent<Function, Module>,
    public detail::UseSiteTraceable<Function, Instruction> {
  Module *Parent;
  bytecode::FunctionType Type;
  llvm::ilist<BasicBlock> BasicBlocks;
  llvm::ilist<Local> Locals;

public:
  explicit Function(Module *Parent);
  Module *getParent() const { return Parent; }
  bytecode::FunctionType const &getType() const { return Type; }

  static llvm::ilist<BasicBlock> Function::*getSublistAccess(BasicBlock *) {
    return &Function::BasicBlocks;
  }
  static llvm::ilist<Local> Function::*getSublistAccess(Local *) {
    return &Function::Locals;
  }
};

class Global :
    public llvm::ilist_node_with_parent<Global, Module>,
    public detail::UseSiteTraceable<Global, Instruction> {
  Module *Parent;
  bytecode::GlobalType Type;

public:
  explicit Global(Module *Parent_);
  Module *getParent() const { return Parent; }
  bytecode::GlobalType const &getType() const { return Type; }
};

class Memory :
    public llvm::ilist_node_with_parent<Memory, Module>,
    public detail::UseSiteTraceable<Memory, Instruction> {
  Module *Parent;
  bytecode::MemoryType Type;

public:
  explicit Memory(Module *Parent_);
  Module *getParent() const { return Parent; }
  bytecode::MemoryType const &getType() const { return Type; }
};

class Table :
    public llvm::ilist_node_with_parent<Table, Module>,
    public detail::UseSiteTraceable<Table, Instruction> {
  Module *Parent;
  bytecode::TableType Type;

public:
  explicit Table(Module *Parent_);
  Module *getParent() const { return Parent; }
};
} // namespace mir

namespace mir::instructions {
///////////////////////////////// Unreachable //////////////////////////////////
class Unreachable : public Instruction {
public:
  explicit Unreachable(BasicBlock *Parent_);
  static bool classof(Instruction *Inst);
};

//////////////////////////////////// Branch ////////////////////////////////////
class Branch : public Instruction {
  Instruction *Condition;
  BasicBlock *Target;

public:
  Branch(BasicBlock *Parent_, Instruction *Condition_, BasicBlock *Target_);
  Branch(BasicBlock *Parent_, BasicBlock *Target_);
  Branch(Branch const &) = delete;
  Branch(Branch &&) noexcept = delete;
  Branch &operator=(Branch const &) = delete;
  Branch &operator=(Branch &&) noexcept = delete;
  ~Branch() noexcept override;
  bool isConditional() const;
  bool isUnconditional() const;
  Instruction *getCondition() const;
  void setCondition(Instruction *Condition_);
  BasicBlock *getTarget() const;
  void setTarget(BasicBlock *Target_);
  void detach_definition(Instruction *Operand_) noexcept override;
  void detach_definition(BasicBlock *Target_) noexcept override;
  static bool classof(Instruction *Inst);
};

///////////////////////////////// BranchTable //////////////////////////////////
class BranchTable : public Instruction {
  Instruction *Operand;
  BasicBlock *DefaultTarget;
  std::vector<BasicBlock *> Targets;

public:
  BranchTable(
      BasicBlock *Parent_, Instruction *Operand_, BasicBlock *DefaultTarget_,
      llvm::ArrayRef<BasicBlock *> Targets_);
  BranchTable(
      BasicBlock *Parent_, Instruction *Operand_, BasicBlock *DefaultTarget_);
  BranchTable(BranchTable const &) = delete;
  BranchTable(BranchTable &&) noexcept = delete;
  BranchTable &operator=(BranchTable const &) = delete;
  BranchTable &operator=(BranchTable &&) noexcept = delete;
  ~BranchTable() noexcept override;
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  BasicBlock *getDefaultTarget() const;
  void setDefaultTarget(BasicBlock *DefaultTarget_);
  llvm::ArrayRef<BasicBlock *> getTargets() const;
  void setTargets(llvm::ArrayRef<BasicBlock *> Targets_);
  void detach_definition(Instruction *Operand_) noexcept override;
  void detach_definition(BasicBlock *Target_) noexcept override;
  static bool classof(Instruction *Inst);
};

/////////////////////////////////// Return /////////////////////////////////////
class Return : public Instruction {
  Instruction *Operand;

public:
  Return(BasicBlock *Parent_, Instruction *Operand_);
  Return(Return const &) = delete;
  Return(Return &&) noexcept = delete;
  Return &operator=(Return const &) = delete;
  Return &operator=(Return &&) noexcept = delete;
  ~Return() noexcept override;
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Instruction *Operand_) noexcept override;
  static bool classof(Instruction *Inst);
};

//////////////////////////////////// Call //////////////////////////////////////
class Call : public Instruction {
  Function *Target;
  std::vector<Instruction *> Arguments;

public:
  Call(
      BasicBlock *Parent_, Function *Target_,
      llvm::ArrayRef<Instruction *> Arguments_);
  Call(Call const &) = delete;
  Call(Call &&) noexcept = delete;
  Call &operator=(Call const &) = delete;
  Call &operator=(Call &&) noexcept = delete;
  ~Call() noexcept override;
  Function *getTarget() const;
  void setTarget(Function *Target_);
  llvm::ArrayRef<Instruction *> getArguments() const;
  void setArguments(llvm::ArrayRef<Instruction *> Arguments_);
  void detach_definition(Function *Target_) noexcept override;
  void detach_definition(Instruction *Argument_) noexcept override;
  static bool classof(Instruction *Inst);
};

/////////////////////////////// CallIndirect ///////////////////////////////////
class CallIndirect : public Instruction {
  Table *IndirectTable;
  Instruction *Operand;
  bytecode::FunctionType ExpectType;

public:
  CallIndirect(
      BasicBlock *Parent_, Table *IndirectTable_, Instruction *Operand_,
      bytecode::FunctionType ExpectType_);
  CallIndirect(CallIndirect const &) = delete;
  CallIndirect(CallIndirect &&) noexcept = delete;
  CallIndirect &operator=(CallIndirect const &) = delete;
  CallIndirect &operator=(CallIndirect &&) noexcept = delete;
  ~CallIndirect() noexcept override;
  Table *getIndirectTable() const;
  void setIndirectTable(Table *IndirectTable_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  bytecode::FunctionType const &getExpectType() const;
  void setExpectType(bytecode::FunctionType Type_);
  void detach_definition(Table *Table_) noexcept override;
  void detach_definition(Instruction *Operand_) noexcept override;
  static bool classof(Instruction *Inst);
};

////////////////////////////////// Select //////////////////////////////////////
class Select : public Instruction {
  Instruction *Condition;
  Instruction *True;
  Instruction *False;

public:
  Select(
      BasicBlock *Parent_, Instruction *Condition_, Instruction *True_,
      Instruction *False_);
  Select(Select const &) = delete;
  Select(Select &&) noexcept = delete;
  Select &operator=(Select const &) = delete;
  Select &operator=(Select &&) noexcept = delete;
  ~Select() noexcept override;
  Instruction *getCondition() const;
  void setCondition(Instruction *Condition_);
  Instruction *getTrue() const;
  void setTrue(Instruction *True_);
  Instruction *getFalse() const;
  void setFalse(Instruction *False_);
  void detach_definition(Instruction *Operand_) noexcept override;
  static bool classof(Instruction *Inst);
};

//////////////////////////////// LocalGet //////////////////////////////////////
class LocalGet : public Instruction {
  Local *Target;

public:
  LocalGet(BasicBlock *Parent_, Local *Target_);
  LocalGet(LocalGet const &) = delete;
  LocalGet(LocalGet &&) noexcept = delete;
  LocalGet &operator=(LocalGet const &) = delete;
  LocalGet &operator=(LocalGet &&) noexcept = delete;
  ~LocalGet() noexcept override;
  Local *getTarget() const;
  void setTarget(Local *Target_);
  void detach_definition(Local *Local_) noexcept override;
  static bool classof(Instruction *Inst);
};

//////////////////////////////// LocalSet //////////////////////////////////////
class LocalSet : public Instruction {
  Local *Target;
  Instruction *Operand;

public:
  LocalSet(BasicBlock *Parent_, Local *Target_, Instruction *Operand_);
  LocalSet(LocalSet const &) = delete;
  LocalSet(LocalSet &&) noexcept = delete;
  LocalSet &operator=(LocalSet const &) = delete;
  LocalSet &operator=(LocalSet &&) noexcept = delete;
  ~LocalSet() noexcept override;
  Local *getTarget() const;
  void setTarget(Local *Target_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Local *Local_) noexcept override;
  void detach_definition(Instruction *Operand_) noexcept override;
  static bool classof(Instruction *Inst);
};

//////////////////////////////// GlobalGet /////////////////////////////////////
class GlobalGet : public Instruction {
  Global *Target;

public:
  GlobalGet(BasicBlock *Parent_, Global *Target_);
  Global *getTarget() const;
  void setTarget(Global *Target_);
  void detach_definition(Global *Global_) noexcept override;
  static bool classof(Instruction *Inst);
};

//////////////////////////////// GlobalSet /////////////////////////////////////
class GlobalSet : public Instruction {
  Global *Target;
  Instruction *Operand;

public:
  GlobalSet(BasicBlock *Parent_, Global *Target_, Instruction *Operand_);
  Global *getTarget() const;
  void setTarget(Global *Target_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Global *Global_) noexcept override;
  void detach_definition(Instruction *Operand_) noexcept override;
  static bool classof(Instruction *Inst);
};

///////////////////////////////// Constant /////////////////////////////////////
class Constant : public Instruction {
  std::variant<std::int32_t, std::int64_t, float, double> Value;

public:
  Constant(BasicBlock *Parent_, std::int32_t Value_);
  Constant(BasicBlock *Parent_, std::int64_t Value_);
  Constant(BasicBlock *Parent_, float Value_);
  Constant(BasicBlock *Parent_, double Value_);
  std::int32_t getI32() const;
  void setI32(std::int32_t Value_);
  std::int64_t getI64() const;
  void setI64(std::int64_t Value_);
  float getF32() const;
  void setF32(float Value_);
  double getF64() const;
  void setF64(double Value_);
  bytecode::ValueType getValueType() const;
  static bool classof(Instruction *Inst);
};

/////////////////////////////// IntUnaryOp /////////////////////////////////////
enum class IntUnaryOperator : std::uint8_t { Eqz, Clz, Ctz, Popcnt };
class IntUnaryOp : public Instruction {
  IntUnaryOperator Operator;
  Instruction *Operand;

public:
  IntUnaryOp(
      BasicBlock *Parent_, IntUnaryOperator Operator_, Instruction *Operand_);
  IntUnaryOperator getOperator() const;
  void setOperator(IntUnaryOperator Operator_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Instruction *Operand_) noexcept override;
  static bool classof(Instruction *Inst);
};

/////////////////////////////// IntBinaryOp ////////////////////////////////////
// clang-format off
enum class IntBinaryOperator : std::uint8_t {
  Eq, Ne, LtS, LtU, GtS, GtU, LeS, LeU, GeS, GeU,
  Add, Sub, Mul, DivS, DivU, RemS, RemU, And, Or, Xor,
  Shl, ShrS, ShrU, Rotl, Rotr
};
// clang-format on
class IntBinaryOp : public Instruction {
  IntBinaryOperator Operator;
  Instruction *LHS;
  Instruction *RHS;

public:
  IntBinaryOp(
      BasicBlock *Parent_, IntBinaryOperator Operator_, Instruction *LHS_,
      Instruction *RHS_);
  IntBinaryOperator getOperator() const;
  void setOperator(IntBinaryOperator Operator_);
  Instruction *getLHS() const;
  void setLHS(Instruction *LHS_);
  Instruction *getRHS() const;
  void setRHS(Instruction *RHS_);
  void detach_definition(Instruction *Operand_) noexcept override;
  static bool classof(Instruction *Inst);
};

//////////////////////////////// FPUnaryOp /////////////////////////////////////
// clang-format off
enum class FPUnaryOperator : std::uint8_t
{ Abs, Neg, Ceil, Floor, Trunc, Nearest, Sqrt};
// clang-format on
class FPUnaryOp : public Instruction {
  FPUnaryOperator Operator;
  Instruction *Operand;

public:
  FPUnaryOp(
      BasicBlock *Parent_, FPUnaryOperator Operator_, Instruction *Operand_);
  FPUnaryOperator getOperator() const;
  void setOperator(FPUnaryOperator Operator_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Instruction *Operand_) noexcept override;
  static bool classof(Instruction *Inst);
};

/////////////////////////////// FPBinaryOp /////////////////////////////////////
// clang-format off
enum class FPBinaryOperator : std::uint8_t {
  Eq, Ne, Lt, Gt, Le, Ge, Add, Sub, Mul, Div, Min, Max, CopySign
};
// clang-format on
class FPBinaryOp : public Instruction {
  FPBinaryOperator Operator;
  Instruction *LHS;
  Instruction *RHS;

public:
  FPBinaryOp(
      BasicBlock *Parent_, FPBinaryOperator Operator_, Instruction *LHS_,
      Instruction *RHS_);
  FPBinaryOperator getOperator() const;
  void setOperator(FPBinaryOperator Operator_);
  Instruction *getLHS() const;
  void setLHS(Instruction *LHS_);
  Instruction *getRHS() const;
  void setRHS(Instruction *RHS_);
  void detach_definition(Instruction *Operand_) noexcept override;
  static bool classof(Instruction *Inst);
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
  RetType visit(Ptr<Instruction>) { return {}; }
};
} // namespace mir

#endif
