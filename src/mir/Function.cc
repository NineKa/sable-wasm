#include "Function.h"

namespace mir {
Local::Local(bytecode::ValueType Type_)
    : ASTNode(ASTNodeKind::Local), Parent(nullptr), Type(Type_),
      IsParameter(false) {}

bytecode::ValueType const &Local::getType() const { return Type; }
bool Local::isParameter() const { return IsParameter; }
Function *Local::getParent() const { return Parent; }
void Local::detach(ASTNode const *) noexcept { utility::unreachable(); }
bool Local::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::Local;
}

Function::Function(bytecode::FunctionType Type_)
    : ASTNode(ASTNodeKind::Function), Parent(nullptr), Type(std::move(Type_)) {
  for (auto const &ValueType : getType().getParamTypes()) {
    auto *Local = BuildLocal(ValueType);
    Local->IsParameter = true;
  }
}

BasicBlock *Function::BuildBasicBlock() {
  auto *AllocatedBB = new BasicBlock();
  getBasicBlocks().push_back(AllocatedBB);
  return AllocatedBB;
}

BasicBlock *Function::BuildBasicBlockAt(BasicBlock *Pos) {
  auto *AllocatedBB = new BasicBlock();
  getBasicBlocks().insert(Pos->getIterator(), AllocatedBB);
  return AllocatedBB;
}

Local *Function::BuildLocal(bytecode::ValueType Type_) {
  auto *AllocatedLocal = new Local(Type_);
  getLocals().push_back(AllocatedLocal);
  return AllocatedLocal;
}

Local *Function::BuildLocalAt(bytecode::ValueType Type_, Local *Pos) {
  auto *AllocatedLocal = new Local(Type_);
  getLocals().insert(Pos->getIterator(), AllocatedLocal);
  return AllocatedLocal;
}

// clang-format off
bool Function::hasBody() const { return !BasicBlocks.empty(); }
BasicBlock const &Function::getEntryBasicBlock() const 
{ return BasicBlocks.front(); }
BasicBlock &Function::getEntryBasicBlock() { return BasicBlocks.front(); }
// clang-format on

detail::IListAccessWrapper<Function, Local> Function::getLocals() {
  return detail::IListAccessWrapper(this, Locals);
}

detail::IListAccessWrapper<Function, BasicBlock> Function::getBasicBlocks() {
  return detail::IListAccessWrapper(this, BasicBlocks);
}

detail::IListConstAccessWrapper<Function, Local> Function::getLocals() const {
  return detail::IListConstAccessWrapper(this, Locals);
}

detail::IListConstAccessWrapper<Function, BasicBlock>
Function::getBasicBlocks() const {
  return detail::IListConstAccessWrapper(this, BasicBlocks);
}

// clang-format off
Module *Function::getParent() const { return Parent; }
llvm::ilist<BasicBlock> Function::*Function::getSublistAccess(BasicBlock *) 
{ return &Function::BasicBlocks; }
llvm::ilist<Local> Function::*Function::getSublistAccess(Local *) 
{ return &Function::Locals; }
void Function::detach(ASTNode const *) noexcept { utility::unreachable(); }
bool Function::classof(ASTNode const *Node) 
{ return Node->getASTNodeKind() == ASTNodeKind::Function; }
// clang-format on

} // namespace mir