#include "Instruction.h"
#include "BasicBlock.h"
#include "Branch.h"

#include <range/v3/view/filter.hpp>

#include <vector>

using namespace bytecode::valuetypes;

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

// clang-format off
bool Type::isPrimitiveI32() const 
{ return isPrimitive() && (asPrimitive() == I32); }
bool Type::isPrimitiveI64() const 
{ return isPrimitive() && (asPrimitive() == I64); }
bool Type::isPrimitiveF32() const 
{ return isPrimitive() && (asPrimitive() == F32); }
bool Type::isPrimitiveF64() const 
{ return isPrimitive() && (asPrimitive() == F64); }
bool Type::isPrimitiveV128() const 
{ return isPrimitive() && (asPrimitive() == V128); }
// clang-format on

Type Type::BuildPrimitiveI32() { return BuildPrimitive(I32); }
Type Type::BuildPrimitiveI64() { return BuildPrimitive(I64); }
Type Type::BuildPrimitiveF32() { return BuildPrimitive(F32); }
Type Type::BuildPrimitiveF64() { return BuildPrimitive(F64); }
Type Type::BuildPrimitiveV128() { return BuildPrimitive(V128); }

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

SIMD128IntLaneInfo::SIMD128IntLaneInfo(SIMD128IntElementKind ElementKind_)
    : ElementKind(ElementKind_) {}

SIMD128IntElementKind SIMD128IntLaneInfo::getElementKind() const {
  return ElementKind;
}

unsigned SIMD128IntLaneInfo::getNumLane() const {
  switch (ElementKind) {
  case SIMD128IntElementKind::I8: return 16;
  case SIMD128IntElementKind::I16: return 8;
  case SIMD128IntElementKind::I32: return 4;
  case SIMD128IntElementKind::I64: return 2;
  default: utility::unreachable();
  }
}

unsigned SIMD128IntLaneInfo::getLaneWidth() const {
  switch (ElementKind) {
  case SIMD128IntElementKind::I8: return 8;
  case SIMD128IntElementKind::I16: return 16;
  case SIMD128IntElementKind::I32: return 32;
  case SIMD128IntElementKind::I64: return 64;
  default: utility::unreachable();
  }
}

SIMD128IntLaneInfo SIMD128IntLaneInfo::widen() const {
  switch (ElementKind) {
  case SIMD128IntElementKind::I8:
    return SIMD128IntLaneInfo(SIMD128IntElementKind::I16);
  case SIMD128IntElementKind::I16:
    return SIMD128IntLaneInfo(SIMD128IntElementKind::I32);
  case SIMD128IntElementKind::I32:
    return SIMD128IntLaneInfo(SIMD128IntElementKind::I64);
  default: utility::unreachable();
  }
}

SIMD128IntLaneInfo SIMD128IntLaneInfo::narrow() const {
  switch (ElementKind) {
  case SIMD128IntElementKind::I16:
    return SIMD128IntLaneInfo(SIMD128IntElementKind::I8);
  case SIMD128IntElementKind::I32:
    return SIMD128IntLaneInfo(SIMD128IntElementKind::I16);
  case SIMD128IntElementKind::I64:
    return SIMD128IntLaneInfo(SIMD128IntElementKind::I32);
  default: utility::unreachable();
  }
}

// clang-format off
SIMD128IntLaneInfo SIMD128IntLaneInfo::getI8x16() 
{ return SIMD128IntLaneInfo(SIMD128IntElementKind::I8); }
SIMD128IntLaneInfo SIMD128IntLaneInfo::getI16x8()
{ return SIMD128IntLaneInfo(SIMD128IntElementKind::I16); }
SIMD128IntLaneInfo SIMD128IntLaneInfo::getI32x4()
{ return SIMD128IntLaneInfo(SIMD128IntElementKind::I32); }
SIMD128IntLaneInfo SIMD128IntLaneInfo::getI64x2()
{ return SIMD128IntLaneInfo(SIMD128IntElementKind::I64); }
// clang-format on

SIMD128FPLaneInfo::SIMD128FPLaneInfo(SIMD128FPElementKind ElementKind_)
    : ElementKind(ElementKind_) {}

SIMD128FPElementKind SIMD128FPLaneInfo::getElementKind() const {
  return ElementKind;
}

unsigned SIMD128FPLaneInfo::getNumLane() const {
  switch (ElementKind) {
  case SIMD128FPElementKind::F32: return 4;
  case SIMD128FPElementKind::F64: return 2;
  default: utility::unreachable();
  }
}

unsigned SIMD128FPLaneInfo::getLaneWidth() const {
  switch (ElementKind) {
  case SIMD128FPElementKind::F32: return 32;
  case SIMD128FPElementKind::F64: return 64;
  default: utility::unreachable();
  }
}

SIMD128IntLaneInfo SIMD128FPLaneInfo::getCmpResultLaneInfo() const {
  switch (ElementKind) {
  case SIMD128FPElementKind::F32:
    return SIMD128IntLaneInfo(SIMD128IntElementKind::I32);
  case SIMD128FPElementKind::F64:
    return SIMD128IntLaneInfo(SIMD128IntElementKind::I64);
  default: utility::unreachable();
  }
}

// clang-format off
SIMD128FPLaneInfo SIMD128FPLaneInfo::getF32x4() 
{ return SIMD128FPLaneInfo(SIMD128FPElementKind::F32); }
SIMD128FPLaneInfo SIMD128FPLaneInfo::getF64x2()
{ return SIMD128FPLaneInfo(SIMD128FPElementKind::F64); }
// clang-format on

bool Instruction::isPhi() const { return instructions::Phi::classof(this); }

bool Instruction::isBranching() const {
  return is_a<instructions::Branch>(this);
}

bool Instruction::isTerminating() const {
  switch (getInstructionKind()) {
  case InstructionKind::Unreachable:
  case InstructionKind::Branch:
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
