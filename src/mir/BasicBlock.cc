#include "BasicBlock.h"
#include "Instruction.h"

namespace mir {
BasicBlock::reference BasicBlock::front() { return Instructions.front(); }
BasicBlock::const_reference BasicBlock::front() const {
  return Instructions.front();
}

BasicBlock::reference BasicBlock::back() { return Instructions.back(); }
BasicBlock::const_reference BasicBlock::back() const {
  return Instructions.back();
}

BasicBlock::BasicBlock(Function *Parent_)
    : ASTNode(ASTNodeKind::BasicBlock), Parent(Parent_) {}

BasicBlock::iterator BasicBlock::insert(iterator Pos, pointer InstPtr) {
  InstPtr->Parent = this;
  return Instructions.insert(Pos, InstPtr);
}

BasicBlock::iterator BasicBlock::insertAfter(iterator Pos, pointer InstPtr) {
  InstPtr->Parent = this;
  return Instructions.insertAfter(Pos, InstPtr);
}

void BasicBlock::splice(iterator Pos, BasicBlock &Other) {
  for (auto &Instruction : Other) Instruction.Parent = this;
  Instructions.splice(Pos, Other.Instructions);
}

void BasicBlock::splice(
    iterator Pos, BasicBlock &Other, iterator Begin, iterator End) {
  for (auto I = Begin; I != End; ++I) (*I).Parent = this;
  Instructions.splice(Pos, Other.Instructions, Begin, End);
}

void BasicBlock::splice(iterator Pos, pointer InstPtr) {
  auto &BasicBlock = *InstPtr->getParent();
  Instructions.splice(Pos, BasicBlock.Instructions, InstPtr);
}

BasicBlock::pointer BasicBlock::remove(pointer InstPtr) {
  return Instructions.remove(InstPtr);
}

void BasicBlock::erase(pointer Inst) { Instructions.erase(Inst); }

void BasicBlock::push_back(pointer Inst) {
  Inst->Parent = this;
  Instructions.push_back(Inst);
}

void BasicBlock::clear() { Instructions.clear(); }

// clang-format off
BasicBlock::iterator BasicBlock::begin()  
{ return Instructions.begin(); }
BasicBlock::const_iterator BasicBlock::begin() const 
{ return Instructions.begin(); }
BasicBlock::const_iterator BasicBlock::cbegin() const 
{ return Instructions.begin(); }
BasicBlock::iterator BasicBlock::end() 
{ return Instructions.end(); }
BasicBlock::const_iterator BasicBlock::end() const 
{ return Instructions.end(); }
BasicBlock::const_iterator BasicBlock::cend() const 
{ return Instructions.end(); }
BasicBlock::reverse_iterator BasicBlock::rbegin() 
{ return Instructions.rbegin(); }
BasicBlock::const_reverse_iterator BasicBlock::rbegin() const 
{ return Instructions.rbegin(); }
BasicBlock::const_reverse_iterator BasicBlock::crbegin() const 
{ return Instructions.rbegin(); }
BasicBlock::reverse_iterator BasicBlock::rend() 
{ return Instructions.rend(); }
BasicBlock::const_reverse_iterator BasicBlock::rend() const 
{ return Instructions.rend(); }
BasicBlock::const_reverse_iterator BasicBlock::crend() const 
{ return Instructions.rend(); }
// clang-format on

bool BasicBlock::empty() const { return Instructions.empty(); }
BasicBlock::size_type BasicBlock::size() const { return Instructions.size(); }
BasicBlock::size_type BasicBlock::max_size() const {
  return Instructions.max_size();
}

std::vector<BasicBlock *> BasicBlock::getInwardFlow() const {
  std::vector<BasicBlock *> Result;
  for (auto const *ASTNode : this->getUsedSites()) {
    if (!is_a<Instruction>(ASTNode)) continue;
    auto *CastedPtr = dyn_cast<Instruction>(ASTNode);
    if (CastedPtr->isBranching()) Result.push_back(CastedPtr->getParent());
  }
  return Result;
}

namespace {
namespace detail {
struct OutwardFlowVisitor :
    mir::InstVisitorBase<OutwardFlowVisitor, std::vector<BasicBlock *>> {
  using ResultType = std::vector<BasicBlock *>;
  ResultType operator()(instructions::Unreachable const *) { return {}; }
  ResultType operator()(instructions::Return const *) { return {}; }
  ResultType operator()(instructions::Branch const *Inst) {
    ResultType Out;
    Out.reserve(2);
    if (Inst->getTarget()) Out.push_back(Inst->getTarget());
    if (Inst->getFalseTarget()) Out.push_back(Inst->getFalseTarget());
    return Out;
  };
  ResultType operator()(instructions::BranchTable const *Inst) {
    ResultType Out;
    if (Inst->getDefaultTarget()) Out.push_back(Inst->getDefaultTarget());
    for (auto *Target : Inst->getTargets())
      if (Target) Out.push_back(Target);
    return Out;
  }
  template <mir::instruction T> ResultType operator()(T const *) {
    utility::unreachable();
  }
};
} // namespace detail
} // namespace

std::vector<BasicBlock *> BasicBlock::getOutwardFlow() const {
  detail::OutwardFlowVisitor Visitor;
  return Visitor(std::addressof(back()));
}

llvm::ilist<Instruction> BasicBlock::*
BasicBlock::getSublistAccess(Instruction *) {
  return &BasicBlock::Instructions;
}

void BasicBlock::detach(ASTNode const *) noexcept { utility::unreachable(); }
bool BasicBlock::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::BasicBlock;
}
} // namespace mir