#ifndef SABLE_INCLUDE_GUARD_MIR_IR
#define SABLE_INCLUDE_GUARD_MIR_IR

#include "../bytecode/Type.h"
#include "ASTNode.h"

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>

#include <span>
#include <variant>

namespace mir {
class Module;

class Function;
class Global;
class Table;
class Memory;

class BasicBlock;
class Local;

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
  MemoryGuard,
  MemoryGrow,
  MemorySize,
  Cast,
  Extend,
  Pack,
  Unpack,
  Phi
};

class Instruction :
    public ASTNode,
    public llvm::ilist_node_with_parent<Instruction, BasicBlock> {
  friend class BasicBlock;
  friend class llvm::ilist_callback_traits<mir::Instruction>;
  InstructionKind Kind;
  BasicBlock *Parent = nullptr;

protected:
  explicit Instruction(InstructionKind Kind_)
      : ASTNode(ASTNodeKind::Instruction), Kind(Kind_) {}

public:
  BasicBlock *getParent() const { return Parent; }
  InstructionKind getInstructionKind() const { return Kind; }

  bool isPhi() const;
  bool isBranching() const;
  bool isTerminating() const;

  static bool classof(ASTNode const *Node);

  void replaceAllUseWith(Instruction *ReplaceValue);
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

template <instruction T> bool is_a(Instruction const &Inst) {
  return T::classof(std::addressof(Inst));
}

template <instruction T> T *dyn_cast(Instruction *Inst) {
  if (Inst == nullptr) return nullptr;
  assert(is_a<T>(Inst));
  return static_cast<T *>(Inst);
}

template <instruction T> T &dyn_cast(Instruction &Inst) {
  assert(is_a<T>(Inst));
  return static_cast<T &>(Inst);
}

template <instruction T> T const *dyn_cast(Instruction const *Inst) {
  if (Inst == nullptr) return nullptr;
  assert(is_a<T>(Inst));
  return static_cast<T const *>(Inst);
}

template <instruction T> T const &dyn_cast(Instruction const &Inst) {
  assert(is_a<T>(Inst));
  return static_cast<T const &>(Inst);
}

} // namespace mir

namespace mir::instructions {
///////////////////////////////// Unreachable //////////////////////////////////
class Unreachable : public Instruction {
public:
  Unreachable();
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
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
      Instruction *Condition_, BasicBlock *TargetTrue_,
      BasicBlock *TargetFalse_);
  explicit Branch(BasicBlock *Target_);
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
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
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
      Instruction *Operand_, BasicBlock *DefaultTarget_,
      std::span<BasicBlock *const> Targets_);
  BranchTable(BranchTable const &) = delete;
  BranchTable(BranchTable &&) noexcept = delete;
  BranchTable &operator=(BranchTable const &) = delete;
  BranchTable &operator=(BranchTable &&) noexcept = delete;
  ~BranchTable() noexcept override;
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  BasicBlock *getDefaultTarget() const;
  void setDefaultTarget(BasicBlock *DefaultTarget_);
  std::size_t getNumTargets() const;
  BasicBlock *getTarget(std::size_t Index) const;
  std::span<BasicBlock *const> getTargets() const;
  void setTarget(std::size_t Index, BasicBlock *Target);
  void setTargets(std::span<BasicBlock *const> Targets_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

/////////////////////////////////// Return /////////////////////////////////////
class Return : public Instruction {
  Instruction *Operand;

public:
  Return();
  explicit Return(Instruction *Operand_);
  Return(Return const &) = delete;
  Return(Return &&) noexcept = delete;
  Return &operator=(Return const &) = delete;
  Return &operator=(Return &&) noexcept = delete;
  ~Return() noexcept override;
  bool hasReturnValue() const;
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

//////////////////////////////////// Call //////////////////////////////////////
class Call : public Instruction {
  Function *Target;
  std::vector<Instruction *> Arguments;

public:
  Call(Function *Target_, std::span<Instruction *const> Arguments_);
  Call(Call const &) = delete;
  Call(Call &&) noexcept = delete;
  Call &operator=(Call const &) = delete;
  Call &operator=(Call &&) noexcept = delete;
  ~Call() noexcept override;
  Function *getTarget() const;
  void setTarget(Function *Target_);
  std::span<Instruction *const> getArguments() const;
  std::size_t getNumArguments() const;
  Instruction *getArgument(std::size_t Index) const;
  void setArguments(std::span<Instruction *const> Arguments_);
  void setArgument(std::size_t Index, Instruction *Argument);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

/////////////////////////////// CallIndirect ///////////////////////////////////
class CallIndirect : public Instruction {
  Table *IndirectTable;
  Instruction *Operand;
  bytecode::FunctionType ExpectType;
  std::vector<Instruction *> Arguments;

public:
  CallIndirect(
      Table *IndirectTable_, Instruction *Operand_,
      bytecode::FunctionType ExpectType_,
      std::span<Instruction *const> Arguments_);
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
  std::span<Instruction *const> getArguments() const;
  std::size_t getNumArguments() const;
  Instruction *getArgument(std::size_t Index) const;
  void setArguments(std::span<Instruction *const> Arguments_);
  void setArgument(std::size_t Index, Instruction *Argument);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

////////////////////////////////// Select //////////////////////////////////////
class Select : public Instruction {
  Instruction *Condition;
  Instruction *True;
  Instruction *False;

public:
  Select(Instruction *Condition_, Instruction *True_, Instruction *False_);
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
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

//////////////////////////////// LocalGet //////////////////////////////////////
class LocalGet : public Instruction {
  Local *Target;

public:
  explicit LocalGet(Local *Target_);
  LocalGet(LocalGet const &) = delete;
  LocalGet(LocalGet &&) noexcept = delete;
  LocalGet &operator=(LocalGet const &) = delete;
  LocalGet &operator=(LocalGet &&) noexcept = delete;
  ~LocalGet() noexcept override;
  Local *getTarget() const;
  void setTarget(Local *Target_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

//////////////////////////////// LocalSet //////////////////////////////////////
class LocalSet : public Instruction {
  Local *Target;
  Instruction *Operand;

public:
  LocalSet(Local *Target_, Instruction *Operand_);
  LocalSet(LocalSet const &) = delete;
  LocalSet(LocalSet &&) noexcept = delete;
  LocalSet &operator=(LocalSet const &) = delete;
  LocalSet &operator=(LocalSet &&) noexcept = delete;
  ~LocalSet() noexcept override;
  Local *getTarget() const;
  void setTarget(Local *Target_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

//////////////////////////////// GlobalGet /////////////////////////////////////
class GlobalGet : public Instruction {
  Global *Target;

public:
  explicit GlobalGet(Global *Target_);
  GlobalGet(GlobalGet const &) = delete;
  GlobalGet(GlobalGet &&) noexcept = delete;
  GlobalGet &operator=(GlobalGet const &) = delete;
  GlobalGet &operator=(GlobalGet &&) noexcept = delete;
  ~GlobalGet() noexcept override;
  Global *getTarget() const;
  void setTarget(Global *Target_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

//////////////////////////////// GlobalSet /////////////////////////////////////
class GlobalSet : public Instruction {
  Global *Target;
  Instruction *Operand;

public:
  GlobalSet(Global *Target_, Instruction *Operand_);
  GlobalSet(GlobalSet const &) = delete;
  GlobalSet(GlobalSet &&) noexcept = delete;
  GlobalSet &operator=(GlobalSet const &) = delete;
  GlobalSet &operator=(GlobalSet &&) noexcept = delete;
  ~GlobalSet() noexcept override;
  Global *getTarget() const;
  void setTarget(Global *Target_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

///////////////////////////////// Constant /////////////////////////////////////
class Constant : public Instruction {
  std::variant<std::int32_t, std::int64_t, float, double> Value;

public:
  explicit Constant(std::int32_t Value_);
  explicit Constant(std::int64_t Value_);
  explicit Constant(float Value_);
  explicit Constant(double Value_);
  std::int32_t &asI32();
  std::int64_t &asI64();
  float &asF32();
  double &asF64();
  std::int32_t asI32() const;
  std::int64_t asI64() const;
  float asF32() const;
  double asF64() const;
  bytecode::ValueType getValueType() const;
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

/////////////////////////////// IntUnaryOp /////////////////////////////////////
enum class IntUnaryOperator : std::uint8_t { Eqz, Clz, Ctz, Popcnt };
class IntUnaryOp : public Instruction {
  IntUnaryOperator Operator;
  Instruction *Operand;

public:
  IntUnaryOp(IntUnaryOperator Operator_, Instruction *Operand_);
  IntUnaryOp(IntUnaryOp const &) = delete;
  IntUnaryOp(IntUnaryOp &&) noexcept = delete;
  IntUnaryOp &operator=(IntUnaryOp const &) = delete;
  IntUnaryOp &operator=(IntUnaryOp &&) noexcept = delete;
  ~IntUnaryOp() noexcept override;
  IntUnaryOperator getOperator() const;
  void setOperator(IntUnaryOperator Operator_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
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
      IntBinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_);
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
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
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
  FPUnaryOp(FPUnaryOperator Operator_, Instruction *Operand_);
  FPUnaryOp(FPUnaryOp const &) = delete;
  FPUnaryOp(FPUnaryOp &&) = delete;
  FPUnaryOp &operator=(FPUnaryOp const &) = delete;
  FPUnaryOp &operator=(FPUnaryOp &&) noexcept = delete;
  ~FPUnaryOp() noexcept override;
  FPUnaryOperator getOperator() const;
  void setOperator(FPUnaryOperator Operator_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
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
  FPBinaryOp(FPBinaryOperator Operator_, Instruction *LHS_, Instruction *RHS_);
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
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
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
      Memory *LinearMemory_, bytecode::ValueType Type_, Instruction *Address_,
      unsigned LoadWidth_);
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
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
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
      Memory *LinearMemory_, Instruction *Address_, Instruction *Operand_,
      unsigned StoreWidth_);
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
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

////////////////////////////// MemoryGuard /////////////////////////////////////
class MemoryGuard : public Instruction {
  Memory *LinearMemory;
  Instruction *Address;
  std::uint32_t GuardSize;

public:
  MemoryGuard(
      Memory *LinearMemory_, Instruction *Address_, std::uint32_t GuardSize_);
  MemoryGuard(MemoryGuard const &) = delete;
  MemoryGuard(MemoryGuard &&) noexcept = delete;
  MemoryGuard &operator=(MemoryGuard const &) = delete;
  MemoryGuard &operator=(MemoryGuard &&) noexcept = delete;
  ~MemoryGuard() noexcept override;
  Memory *getLinearMemory() const;
  void setLinearMemory(Memory *LinearMemory_);
  Instruction *getAddress() const;
  void setAddress(Instruction *Address_);
  std::uint32_t getGuardSize() const;
  void setGuardSize(std::uint32_t GuardSize_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

/////////////////////////////// MemoryGrow /////////////////////////////////////
class MemoryGrow : public Instruction {
  Memory *LinearMemory;
  Instruction *Size;

public:
  MemoryGrow(Memory *LinearMemory_, Instruction *Size_);
  MemoryGrow(MemoryGrow const &) = delete;
  MemoryGrow(MemoryGrow &&) noexcept = delete;
  MemoryGrow &operator=(MemoryGrow const &) = delete;
  MemoryGrow &operator=(MemoryGrow &&) noexcept = delete;
  ~MemoryGrow() noexcept override;
  Memory *getLinearMemory() const;
  void setLinearMemory(Memory *LinearMemory_);
  Instruction *getSize() const;
  void setSize(Instruction *Size_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

/////////////////////////////// MemorySize /////////////////////////////////////
class MemorySize : public Instruction {
  Memory *LinearMemory;

public:
  explicit MemorySize(Memory *LinearMemory_);
  MemorySize(MemorySize const &) = delete;
  MemorySize(MemorySize &&) noexcept = delete;
  MemorySize &operator=(MemorySize const &) = delete;
  MemorySize &operator=(MemorySize &&) noexcept = delete;
  ~MemorySize() noexcept override;
  Memory *getLinearMemory() const;
  void setLinearMemory(Memory *LinearMemory_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

////////////////////////////////// Cast ////////////////////////////////////////
enum class CastMode {
  Conversion,
  ConversionSigned,
  ConversionUnsigned,
  Reinterpret,
  SatConversionSigned,
  SatConversionUnsigned
};

class Cast : public Instruction {
  CastMode Mode;
  bytecode::ValueType Type;
  Instruction *Operand;

public:
  Cast(CastMode Mode_, bytecode::ValueType Type_, Instruction *Operand_);
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
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

///////////////////////////////// Extend ///////////////////////////////////////
class Extend : public Instruction {
  Instruction *Operand;
  unsigned FromWidth;

public:
  Extend(Instruction *Operand_, unsigned FromWidth_);
  Extend(Extend const &) = delete;
  Extend(Extend &&) noexcept = delete;
  Extend &operator=(Extend const &) = delete;
  Extend &operator=(Extend &&) noexcept = delete;
  ~Extend() noexcept override;
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  unsigned getFromWidth() const;
  void setFromWidth(unsigned FromWidth_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

///////////////////////////////// Packed ///////////////////////////////////////
class Pack : public Instruction {
  std::vector<Instruction *> Arguments;

public:
  explicit Pack(std::span<Instruction *const> Arguments_);
  Pack(Pack const &) = delete;
  Pack(Pack &&) noexcept = delete;
  Pack &operator=(Pack const &) = delete;
  Pack &operator=(Pack &&) noexcept = delete;
  ~Pack() noexcept override;
  std::size_t getNumArguments() const;
  Instruction *getArgument(std::size_t Index) const;
  std::span<Instruction *const> getArguments() const;
  void setArguments(std::span<Instruction *const> Arguments_);
  void setArgument(std::size_t Index, Instruction *Argument);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

///////////////////////////////// Unpack ///////////////////////////////////////
class Unpack : public Instruction {
  unsigned Index;
  Instruction *Operand;

public:
  Unpack(Instruction *Operand_, unsigned Index_);
  Unpack(Unpack const &) = delete;
  Unpack(Unpack &&) = delete;
  Unpack &operator=(Unpack const &) = delete;
  Unpack &operator=(Unpack &&) noexcept = delete;
  ~Unpack() noexcept override;
  unsigned getIndex() const;
  void setIndex(unsigned Index_);
  Instruction *getOperand() const;
  void setOperand(Instruction *Operand_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

/////////////////////////////////// Phi ////////////////////////////////////////
class Phi : public Instruction {
  std::vector<std::pair<Instruction *, BasicBlock *>> Candidates;
  bytecode::ValueType Type;

public:
  explicit Phi(bytecode::ValueType Type_);
  Phi(Phi const &) = delete;
  Phi(Phi &&) noexcept = delete;
  Phi &operator=(Phi const &) = delete;
  Phi &operator=(Phi &&) noexcept = delete;
  ~Phi() noexcept override;

  std::span<std::pair<Instruction *, BasicBlock *> const> getCandidates() const;
  std::size_t getNumCandidates() const;
  std::pair<Instruction *, BasicBlock *> getCandidate(std::size_t Index) const;
  void setCandidates(
      std::span<std::pair<Instruction *, BasicBlock *> const> Candidates_);
  void setCandidate(std::size_t Index, Instruction *Value, BasicBlock *Flow);
  void addCandidate(Instruction *Value_, BasicBlock *Path_);
  bytecode::ValueType getType() const;
  void setType(bytecode::ValueType Type_);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};
} // namespace mir::instructions

namespace mir {
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
    case IKind::MemoryGuard : return castAndCall<MemoryGuard>(Inst);
    case IKind::MemoryGrow  : return castAndCall<MemoryGrow>(Inst);
    case IKind::MemorySize  : return castAndCall<MemorySize>(Inst);
    case IKind::Cast        : return castAndCall<Cast>(Inst);
    case IKind::Extend      : return castAndCall<Extend>(Inst);
    case IKind::Pack        : return castAndCall<Pack>(Inst);
    case IKind::Unpack      : return castAndCall<Unpack>(Inst);
    case IKind::Phi         : return castAndCall<Phi>(Inst);
    default: utility::unreachable();
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
