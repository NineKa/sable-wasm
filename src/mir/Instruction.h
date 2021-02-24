#ifndef SABLE_INCLUDE_GUARD_MIR_IR
#define SABLE_INCLUDE_GUARD_MIR_IR

#include "../bytecode/Type.h"
#include "ASTNode.h"

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
  FPBinaryOp,
  Load,
  Store,
  MemorySize,
  MemoryGrow,
  MemoryGuard,
  Cast,
  Extend,
  Pack,
  Unpack,
  Phi
};

class Instruction :
    public ASTNode,
    public llvm::ilist_node_with_parent<Instruction, BasicBlock>,
    public detail::UseSiteTraceable<Instruction, Instruction> {
  InstructionKind Kind;
  BasicBlock *Parent = nullptr;

protected:
  explicit Instruction(InstructionKind Kind_, BasicBlock *Parent_)
      : ASTNode(ASTNodeKind::Instruction), Kind(Kind_), Parent(Parent_) {}

public:
  Instruction(Instruction const &) = delete;
  Instruction(Instruction &&) noexcept = delete;
  Instruction &operator=(Instruction const &) = delete;
  Instruction &operator=(Instruction &&) noexcept = delete;
  ~Instruction() noexcept override = default;
  BasicBlock *getParent() const { return Parent; }
  InstructionKind getInstructionKind() const { return Kind; }

  auto getUsedSites() {
    return ranges::subrange(use_site_begin(), use_site_end());
  }

  // called when defined entity is removed from IR
  // a proper instruction should also remove them from operands (set to nullptr)
  // defaults to do nothing
  // called from destructors, must be noexcept
  // TODO: move these to traits
  virtual void detach_definition(Instruction const *) noexcept {}
  virtual void detach_definition(Local const *) noexcept {}
  virtual void detach_definition(BasicBlock const *) noexcept {}
  virtual void detach_definition(Function const *) noexcept {}
  virtual void detach_definition(Global const *) noexcept {}
  virtual void detach_definition(Memory const *) noexcept {}
  virtual void detach_definition(Table const *) noexcept {}

  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Instruction;
  }
};

template <typename T>
concept instruction = ast_node<T> &&
    std::derived_from<T, Instruction> &&requires() {
  { T::classof(std::declval<Instruction const *>()) }
  ->std::convertible_to<bool>;
};

template <instruction T> bool is_a(Instruction const *Inst) {
  return T::classof(Inst);
}

template <instruction T> T *dyn_cast(Instruction *Inst) {
  assert(is_a<T>(Inst));
  return static_cast<T *>(Inst);
}

template <instruction T> T const *dyn_cast(Instruction const *Inst) {
  assert(is_a<T>(Inst));
  return static_cast<T const *>(Inst);
}

class BasicBlock :
    public ASTNode,
    public llvm::ilist_node_with_parent<BasicBlock, Function>,
    public detail::UseSiteTraceable<BasicBlock, Instruction> {
  Function *Parent;
  llvm::ilist<Instruction> Instructions;

public:
  explicit BasicBlock(Function *Parent_);

  template <std::derived_from<Instruction> T, typename... ArgTypes>
  T *BuildInst(ArgTypes &&...Args) {
    auto *Inst = new T(this, std::forward<ArgTypes>(Args)...);
    Instructions.push_back(Inst);
    return Inst;
  }

  template <std::derived_from<Instruction> T, typename... ArgTypes>
  T *BuildInst(Instruction *Before, ArgTypes &&...Args) {
    auto *Inst = new T(this, std::forward<ArgTypes>(Args)...);
    Instructions.insert(Before->getIterator(), Inst);
    return Inst;
  }

  void erase(Instruction *Inst) { Instructions.erase(Inst); }

  Function *getParent() const { return Parent; }

  using iterator = decltype(Instructions)::iterator;
  using const_iterator = decltype(Instructions)::const_iterator;
  iterator begin() { return Instructions.begin(); }
  iterator end() { return Instructions.end(); }
  const_iterator begin() const { return Instructions.begin(); }
  const_iterator end() const { return Instructions.end(); }

  auto getUsedSites() {
    return ranges::subrange(use_site_begin(), use_site_end());
  }

  static llvm::ilist<Instruction> BasicBlock::*getSublistAccess(Instruction *);

  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::BasicBlock;
  }
};
} // namespace mir

namespace mir::instructions {
///////////////////////////////// Unreachable //////////////////////////////////
class Unreachable : public Instruction {
public:
  explicit Unreachable(BasicBlock *Parent_);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

//////////////////////////////////// Branch ////////////////////////////////////
class Branch : public Instruction {
  Instruction *Condition;
  BasicBlock *Target;
  BasicBlock *FalseTarget;

public:
  Branch(
      BasicBlock *Parent_, Instruction *Condition_, BasicBlock *TargetTrue_,
      BasicBlock *TargetFalse_);
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
  BasicBlock *getFalseTarget() const;
  void setFalseTarget(BasicBlock *FalseTarget_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  void detach_definition(BasicBlock const *Target_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  void detach_definition(Instruction const *Operand_) noexcept override;
  void detach_definition(BasicBlock const *Target_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

/////////////////////////////////// Return /////////////////////////////////////
class Return : public Instruction {
  Instruction *Operand;

public:
  explicit Return(BasicBlock *Parent_);
  Return(BasicBlock *Parent_, Instruction *Operand_);
  Return(Return const &) = delete;
  Return(Return &&) noexcept = delete;
  Return &operator=(Return const &) = delete;
  Return &operator=(Return &&) noexcept = delete;
  ~Return() noexcept override;
  bool hasReturnValue() const;
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  void detach_definition(Function const *Target_) noexcept override;
  void detach_definition(Instruction const *Argument_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  void detach_definition(Table const *Table_) noexcept override;
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  void detach_definition(Local const *Local_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  void detach_definition(Local const *Local_) noexcept override;
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

//////////////////////////////// GlobalGet /////////////////////////////////////
class GlobalGet : public Instruction {
  Global *Target;

public:
  GlobalGet(BasicBlock *Parent_, Global *Target_);
  GlobalGet(GlobalGet const &) = delete;
  GlobalGet(GlobalGet &&) noexcept = delete;
  GlobalGet &operator=(GlobalGet const &) = delete;
  GlobalGet &operator=(GlobalGet &&) noexcept = delete;
  ~GlobalGet() noexcept override;
  Global *getTarget() const;
  void setTarget(Global *Target_);
  void detach_definition(Global const *Global_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

//////////////////////////////// GlobalSet /////////////////////////////////////
class GlobalSet : public Instruction {
  Global *Target;
  Instruction *Operand;

public:
  GlobalSet(BasicBlock *Parent_, Global *Target_, Instruction *Operand_);
  GlobalSet(GlobalSet const &) = delete;
  GlobalSet(GlobalSet &&) noexcept = delete;
  GlobalSet &operator=(GlobalSet const &) = delete;
  GlobalSet &operator=(GlobalSet &&) noexcept = delete;
  ~GlobalSet() noexcept override;
  Global *getTarget() const;
  void setTarget(Global *Target_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Global const *Global_) noexcept override;
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

/////////////////////////////// IntUnaryOp /////////////////////////////////////
enum class IntUnaryOperator : std::uint8_t { Eqz, Clz, Ctz, Popcnt };
class IntUnaryOp : public Instruction {
  IntUnaryOperator Operator;
  Instruction *Operand;

public:
  IntUnaryOp(
      BasicBlock *Parent_, IntUnaryOperator Operator_, Instruction *Operand_);
  IntUnaryOp(IntUnaryOp const &) = delete;
  IntUnaryOp(IntUnaryOp &&) noexcept = delete;
  IntUnaryOp &operator=(IntUnaryOp const &) = delete;
  IntUnaryOp &operator=(IntUnaryOp &&) noexcept = delete;
  ~IntUnaryOp() noexcept override;
  IntUnaryOperator getOperator() const;
  void setOperator(IntUnaryOperator Operator_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  IntBinaryOp(IntBinaryOp const &) = delete;
  IntBinaryOp(IntBinaryOp &&) noexcept = delete;
  IntBinaryOp &operator=(IntBinaryOp const &) = delete;
  IntBinaryOp &operator=(IntBinaryOp &&) noexcept = delete;
  ~IntBinaryOp() noexcept override;
  IntBinaryOperator getOperator() const;
  void setOperator(IntBinaryOperator Operator_);
  Instruction *getLHS() const;
  void setLHS(Instruction *LHS_);
  Instruction *getRHS() const;
  void setRHS(Instruction *RHS_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  FPUnaryOp(FPUnaryOp const &) = delete;
  FPUnaryOp(FPUnaryOp &&) = delete;
  FPUnaryOp &operator=(FPUnaryOp const &) = delete;
  FPUnaryOp &operator=(FPUnaryOp &&) noexcept = delete;
  ~FPUnaryOp() noexcept override;
  FPUnaryOperator getOperator() const;
  void setOperator(FPUnaryOperator Operator_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
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
  FPBinaryOp(FPBinaryOp const &) = delete;
  FPBinaryOp(FPBinaryOp &&) noexcept = delete;
  FPBinaryOp &operator=(FPBinaryOp const &) = delete;
  FPBinaryOp &operator=(FPBinaryOp &&) noexcept = delete;
  ~FPBinaryOp() noexcept override;
  FPBinaryOperator getOperator() const;
  void setOperator(FPBinaryOperator Operator_);
  Instruction *getLHS() const;
  void setLHS(Instruction *LHS_);
  Instruction *getRHS() const;
  void setRHS(Instruction *RHS_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

////////////////////////////////// Load ////////////////////////////////////////
class Load : public Instruction {
  Memory *LinearMemory;
  bytecode::ValueType Type;
  Instruction *Address;
  unsigned LoadWidth;

public:
  Load(
      BasicBlock *Parent_, Memory *LinearMemory_, bytecode::ValueType Type_,
      Instruction *Address_, unsigned LoadWidth_);
  Load(Load const &) = delete;
  Load(Load &&) noexcept = delete;
  Load &operator=(Load const &) = delete;
  Load &operator=(Load &&) noexcept = delete;
  ~Load() noexcept override;
  Memory *getLinearMemory() const;
  void setLinearMemory(Memory *LinearMemory_);
  bytecode::ValueType const &getType() const;
  void setType(bytecode::ValueType const &Type_);
  Instruction *getAddress() const;
  void setAddress(Instruction *Address_);
  unsigned getLoadWidth() const;
  void setLoadWidth(unsigned LoadWidth_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  void detach_definition(Memory const *Memory_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

///////////////////////////////// Store ////////////////////////////////////////
class Store : public Instruction {
  Memory *LinearMemory;
  Instruction *Address;
  Instruction *Operand;
  unsigned StoreWidth;

public:
  Store(
      BasicBlock *Parent_, Memory *LinearMemory_, Instruction *Address_,
      Instruction *Operand_, unsigned StoreWidth_);
  Store(Store const &) = delete;
  Store(Store &&) noexcept = delete;
  Store &operator=(Store const &) = delete;
  Store &operator=(Store &&) noexcept = delete;
  ~Store() noexcept override;
  Memory *getLinearMemory() const;
  void setLinearMemory(Memory *LinearMemory_);
  Instruction *getAddress() const;
  void setAddress(Instruction *Address_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  unsigned getStoreWidth() const;
  void setStoreWidth(unsigned StoreWidth_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  void detach_definition(Memory const *Memory_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

////////////////////////////// MemorySize //////////////////////////////////////
class MemorySize : public Instruction {
  Memory *LinearMemory;

public:
  MemorySize(BasicBlock *Parent_, Memory *LinearMemory_);
  MemorySize(MemorySize const &) = delete;
  MemorySize(MemorySize &&) noexcept = delete;
  MemorySize &operator=(MemorySize const &) = delete;
  MemorySize &operator=(MemorySize &&) noexcept = delete;
  ~MemorySize() noexcept override;
  Memory *getLinearMemory() const;
  void setLinearMemory(Memory *LinearMemory_);
  void detach_definition(Memory const *Memory_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

////////////////////////////// MemoryGrow //////////////////////////////////////
class MemoryGrow : public Instruction {
  Memory *LinearMemory;
  Instruction *GrowSize;

public:
  MemoryGrow(
      BasicBlock *Parent_, Memory *LinearMemory_, Instruction *GrowSize_);
  MemoryGrow(MemoryGrow const &) = delete;
  MemoryGrow(MemoryGrow &&) noexcept = delete;
  MemoryGrow &operator=(MemoryGrow const &) = delete;
  MemoryGrow &operator=(MemoryGrow &&) noexcept = delete;
  ~MemoryGrow() noexcept override;
  Memory *getLinearMemory() const;
  void setLinearMemory(Memory *LinearMemory_);
  Instruction *getGrowSize() const;
  void setGrowSize(Instruction *GrowSize_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  void detach_definition(Memory const *Memory_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

////////////////////////////// MemoryGuard /////////////////////////////////////
class MemoryGuard : public Instruction {
  Memory *LinearMemory;
  Instruction *Address;

public:
  MemoryGuard(
      BasicBlock *Parent_, Memory *LinearMemory_, Instruction *Address_);
  MemoryGuard(MemoryGuard const &) = delete;
  MemoryGuard(MemoryGuard &&) noexcept = delete;
  MemoryGuard &operator=(MemoryGuard const &) = delete;
  MemoryGuard &operator=(MemoryGuard &&) noexcept = delete;
  ~MemoryGuard() noexcept override;
  Memory *getLinearMemory() const;
  void setLinearMemory(Memory *LinearMemory_);
  Instruction *getAddress() const;
  void setAddress(Instruction *Address_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  void detach_definition(Memory const *Memory_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

////////////////////////////////// Cast ////////////////////////////////////////
enum class CastMode { Conversion, Reinterpret, SaturatedConversion };
class Cast : public Instruction {
  CastMode Mode;
  bytecode::ValueType Type;
  Instruction *Operand;
  bool IsSigned;

public:
  Cast(
      BasicBlock *Parent_, CastMode Mode_, bytecode::ValueType Type_,
      Instruction *Operand_, bool IsSigned_);
  Cast(Cast const &) = delete;
  Cast(Cast &&) noexcept = delete;
  Cast &operator=(Cast const &) = delete;
  Cast &operator=(Cast &&) noexcept = delete;
  ~Cast() noexcept override;
  CastMode getMode() const;
  void setMode(CastMode Mode_);
  bytecode::ValueType const &getType() const;
  void setType(bytecode::ValueType const &Type_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  bool getIsSigned() const;
  void setIsSigned(bool IsSigned_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

///////////////////////////////// Extend ///////////////////////////////////////
class Extend : public Instruction {
  Instruction *Operand;
  unsigned FromWidth;

public:
  Extend(BasicBlock *Parent_, Instruction *Operand_, unsigned FromWidth_);
  Extend(Extend const &) = delete;
  Extend(Extend &&) noexcept = delete;
  Extend &operator=(Extend const &) = delete;
  Extend &operator=(Extend &&) noexcept = delete;
  ~Extend() noexcept override;
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  unsigned getFromWidth() const;
  void setFromWidth(unsigned FromWidth_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

///////////////////////////////// Packed ///////////////////////////////////////
class Pack : public Instruction {
  std::vector<Instruction *> Arguments;

public:
  Pack(BasicBlock *Parent_, llvm::ArrayRef<Instruction *> Arguments_);
  Pack(Pack const &) = delete;
  Pack(Pack &&) noexcept = delete;
  Pack &operator=(Pack const &) = delete;
  Pack &operator=(Pack &&) noexcept = delete;
  ~Pack() noexcept override;
  llvm::ArrayRef<Instruction *> getArguments() const;
  void setArguments(llvm::ArrayRef<Instruction *> Arguments_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

///////////////////////////////// Unpack ///////////////////////////////////////
class Unpack : public Instruction {
  unsigned Index;
  Instruction *Operand;

public:
  Unpack(BasicBlock *Parent_, unsigned Index_, Instruction *Operand_);
  Unpack(Unpack const &) = delete;
  Unpack(Unpack &&) = delete;
  Unpack &operator=(Unpack const &) = delete;
  Unpack &operator=(Unpack &&) noexcept = delete;
  ~Unpack() noexcept override;
  unsigned getIndex() const;
  void setIndex(unsigned Index_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

/////////////////////////////////// Phi ////////////////////////////////////////
class Phi : public Instruction {
  std::vector<Instruction *> Arguments;

public:
  Phi(BasicBlock *Parent_, llvm::ArrayRef<Instruction *> Arguments_);
  Phi(Phi const &) = delete;
  Phi(Phi &&) noexcept = delete;
  Phi &operator=(Phi const &) = delete;
  Phi &operator=(Phi &&) noexcept = delete;
  ~Phi() noexcept override;
  llvm::ArrayRef<Instruction *> getArguments() const;
  void setArguments(llvm::ArrayRef<Instruction *> Arguments_);
  void detach_definition(Instruction const *Operand_) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

} // namespace mir::instructions

namespace mir {
static_assert(instruction<instructions::Unreachable>);
template <typename Derived, typename RetType = void, bool Const = true>
class InstVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <instruction T> RetType castAndCall(Ptr<Instruction> Inst) {
    return derived()(dyn_cast<T>(Inst));
  }

public:
  RetType visit(Ptr<Instruction> Inst) {
    using IKind = InstructionKind;
    using namespace instructions;
    // clang-format off
    switch (Inst->getInstructionKind()) {
    case IKind::Unreachable : return castAndCall<Unreachable>(Inst);
    case IKind::Branch      : return castAndCall<Branch>(Inst);
    case IKind::BranchTable : return castAndCall<BranchTable>(Inst);
    case IKind::Return      : return castAndCall<Return>(Inst);
    case IKind::Call        : return castAndCall<Call>(Inst);
    case IKind::CallIndirect: return castAndCall<CallIndirect>(Inst);
    case IKind::Select      : return castAndCall<Select>(Inst);
    case IKind::LocalGet    : return castAndCall<LocalGet>(Inst);
    case IKind::LocalSet    : return castAndCall<LocalSet>(Inst);
    case IKind::GlobalGet   : return castAndCall<GlobalGet>(Inst);
    case IKind::GlobalSet   : return castAndCall<GlobalSet>(Inst);
    case IKind::Constant    : return castAndCall<Constant>(Inst);
    case IKind::IntUnaryOp  : return castAndCall<IntUnaryOp>(Inst);
    case IKind::IntBinaryOp : return castAndCall<IntBinaryOp>(Inst);
    case IKind::FPUnaryOp   : return castAndCall<FPUnaryOp>(Inst);
    case IKind::FPBinaryOp  : return castAndCall<FPBinaryOp>(Inst);
    case IKind::Load        : return castAndCall<Load>(Inst);
    case IKind::Store       : return castAndCall<Store>(Inst);
    case IKind::MemorySize  : return castAndCall<MemorySize>(Inst);
    case IKind::MemoryGrow  : return castAndCall<MemoryGrow>(Inst);
    case IKind::MemoryGuard : return castAndCall<MemoryGuard>(Inst);
    case IKind::Cast        : return castAndCall<Cast>(Inst);
    case IKind::Extend      : return castAndCall<Extend>(Inst);
    case IKind::Pack        : return castAndCall<Pack>(Inst);
    case IKind::Unpack      : return castAndCall<Unpack>(Inst);
    case IKind::Phi         : return castAndCall<Phi>(Inst);
    default: SABLE_UNREACHABLE();
    }
    // clang-format on
  }
};
} // namespace mir

namespace fmt {
template <> struct formatter<mir::InstructionKind> {
  static char const *getEnumString(mir::InstructionKind const &Kind);
  template <typename C> auto parse(C &&CTX) { return CTX.begin(); }
  template <typename C> auto format(mir::InstructionKind const &Kind, C &&CTX) {
    return fmt::format_to(CTX.out(), "{}", getEnumString(Kind));
  }
};
} // namespace fmt

#endif
