#include "Instruction.h"
#include "BasicBlock.h"

#include <range/v3/view/filter.hpp>

#include <vector>

namespace mir {
class Type::AggregateStorage {
  std::vector<bytecode::ValueType> Members;

public:
  explicit AggregateStorage(std::span<bytecode::ValueType const> Aggregate)
      : Members(ranges::to<std::vector<bytecode::ValueType>>(Aggregate)) {}
  using iterator = decltype(Members)::const_iterator;
  iterator begin() const { return Members.begin(); }
  iterator end() const { return Members.end(); }
  std::size_t size() const { return Members.size(); }
  std::span<bytecode::ValueType const> asView() const { return Members; }
  bool operator==(AggregateStorage const &Other) const = default;
};

Type::Type(TypeKind Kind_) : Kind(Kind_), Storage(std::monostate()) {}
Type::Type(bytecode::ValueType Primitive)
    : Kind(TypeKind::Primitive), Storage(Primitive) {}
Type::Type(std::span<bytecode::ValueType const> Aggregate)
    : Kind(TypeKind::Aggregate),
      Storage(std::make_shared<AggregateStorage>(Aggregate)) {}

Type::Type(Type const &) = default;
Type::Type(Type &&) noexcept = default;
Type &Type::operator=(Type const &) = default;
Type &Type::operator=(Type &&) noexcept = default;
Type::~Type() noexcept = default;

bool Type::isUnit() const { return Kind == TypeKind::Unit; }
bool Type::isBottom() const { return Kind == TypeKind::Bottom; }
bool Type::isPrimitive() const { return Kind == TypeKind::Primitive; }
bool Type::isAggregate() const { return Kind == TypeKind::Aggregate; }
TypeKind Type::getKind() const { return Kind; }

bytecode::ValueType const &Type::asPrimitive() const {
  assert(isPrimitive());
  return std::get<bytecode::ValueType>(Storage);
}

std::span<bytecode::ValueType const> Type::asAggregate() const {
  assert(isAggregate());
  return std::get<std::shared_ptr<AggregateStorage>>(Storage)->asView();
}

bool Type::isPrimitiveI32() const {
  return isPrimitive() && (asPrimitive() == bytecode::valuetypes::I32);
}

bool Type::isPrimitiveI64() const {
  return isPrimitive() && (asPrimitive() == bytecode::valuetypes::I64);
}

bool Type::isPrimitiveF32() const {
  return isPrimitive() && (asPrimitive() == bytecode::valuetypes::F32);
}

bool Type::isPrimitiveF64() const {
  return isPrimitive() && (asPrimitive() == bytecode::valuetypes::F64);
}

bool Type::isIntegral() const { return isPrimitiveI32() || isPrimitiveI64(); }

bool Type::isFloatingPoint() const {
  return isPrimitiveF32() || isPrimitiveF64();
}

Type Type::BuildUnit() { return Type(TypeKind::Unit); }

Type Type::BuildPrimitive(bytecode::ValueType Primitive) {
  return Type(Primitive);
}

Type Type::BuildAggregate(std::span<bytecode::ValueType const> Aggregate) {
  return Type(Aggregate);
}

Type Type::BuildBottom() { return Type(TypeKind::Bottom); }

bool Type::operator==(Type const &Other) const {
  if (Kind != Other.getKind()) return false;
  switch (Kind) {
  case TypeKind::Unit:
  case TypeKind::Bottom: return true;
  case TypeKind::Primitive: {
    auto const &LHS = std::get<bytecode::ValueType>(Storage);
    auto const &RHS = std::get<bytecode::ValueType>(Other.Storage);
    return LHS == RHS;
  }
  case TypeKind::Aggregate: {
    using AggregateStoragePtr = std::shared_ptr<AggregateStorage>;
    auto const &LHS = std::get<AggregateStoragePtr>(Storage);
    auto const &RHS = std::get<AggregateStoragePtr>(Other.Storage);
    return *LHS == *RHS;
  }
  default: utility::unreachable();
  }
}

bool Instruction::isPhi() const { return instructions::Phi::classof(this); }

bool Instruction::isBranching() const {
  switch (getInstructionKind()) {
  case InstructionKind::Branch:
  case InstructionKind::BranchTable: return true;
  default: return false;
  }
}

bool Instruction::isTerminating() const {
  switch (getInstructionKind()) {
  case InstructionKind::Unreachable:
  case InstructionKind::Branch:
  case InstructionKind::BranchTable:
  case InstructionKind::Return: return true;
  default: return false;
  }
}

bool Instruction::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::Instruction;
}

void Instruction::eraseFromParent() { Parent->erase(this); }

void Instruction::replaceAllUseWith(Instruction *ReplaceValue) const {
  // clang-format off
  auto ReplaceQueue = getUsedSites() 
    | ranges::views::filter([](auto const *Node) {
        return is_a<mir::Instruction>(Node);
      })
    | ranges::to<std::vector<mir::ASTNode *>>();
  // clang-format on
  for (auto *Node : ReplaceQueue) {
    auto *CastedPtr = dyn_cast<Instruction>(Node);
    CastedPtr->replace(this, ReplaceValue);
  }
}
} // namespace mir

namespace fmt {
char const *formatter<mir::InstructionKind>::getEnumString(
    mir::InstructionKind const &Kind) {
  using IKind = mir::InstructionKind;
  // clang-format off
  switch (Kind) {
  case IKind::Unreachable : return "unreachable";
  case IKind::Branch      : return "br";
  case IKind::BranchTable : return "br.table";
  case IKind::Return      : return "ret";
  case IKind::Call        : return "call";
  case IKind::CallIndirect: return "call_indirect";
  case IKind::Select      : return "select";
  case IKind::LocalGet    : return "local.get";
  case IKind::LocalSet    : return "local.set";
  case IKind::GlobalGet   : return "global.get";
  case IKind::GlobalSet   : return "global.set";
  case IKind::Constant    : return "const";
  case IKind::IntUnaryOp  : return "int.unary";
  case IKind::IntBinaryOp : return "int.binary";
  case IKind::FPUnaryOp   : return "fp.unary";
  case IKind::FPBinaryOp  : return "fp.binary";
  case IKind::Load        : return "load";
  case IKind::Store       : return "store";
  case IKind::MemoryGuard : return "memory.guard";
  case IKind::MemoryGrow  : return "memory.grow";
  case IKind::MemorySize  : return "memory.size";
  case IKind::Cast        : return "cast";
  case IKind::Extend      : return "extend";
  case IKind::Pack        : return "pack";
  case IKind::Unpack      : return "unpack";
  case IKind::Phi         : return "phi";
  default: utility::unreachable();
  }
  // clang-format on
}
} // namespace fmt
