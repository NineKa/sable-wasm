#include "BasicBlock.h"
#include "Function.h"
#include "Instruction.h"
#include "Module.h"

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/unique.hpp>

namespace mir {
BasicBlock::BasicBlock() : ASTNode(ASTNodeKind::BasicBlock), Parent(nullptr) {}

BasicBlock::reference BasicBlock::front() { return Instructions.front(); }
BasicBlock::const_reference BasicBlock::front() const {
  return Instructions.front();
}

BasicBlock::reference BasicBlock::back() { return Instructions.back(); }
BasicBlock::const_reference BasicBlock::back() const {
  return Instructions.back();
}

BasicBlock::iterator BasicBlock::insert(iterator Pos, pointer InstPtr) {
  assert(InstPtr->Parent == nullptr);
  InstPtr->Parent = this;
  return Instructions.insert(Pos, InstPtr);
}

BasicBlock::iterator BasicBlock::insertAfter(iterator Pos, pointer InstPtr) {
  assert(InstPtr->Parent == nullptr);
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
  InstPtr->Parent = this;
  auto &BasicBlock = *InstPtr->getParent();
  Instructions.splice(Pos, BasicBlock.Instructions, InstPtr);
}

BasicBlock::pointer BasicBlock::remove(pointer InstPtr) {
  assert(InstPtr->Parent == this);
  auto *Result = Instructions.remove(InstPtr);
  InstPtr->Parent = nullptr;
  return Result;
}

void BasicBlock::erase(pointer Inst) {
  assert(Inst->Parent == this);
  Instructions.erase(Inst);
}

void BasicBlock::push_back(pointer Inst) {
  assert(Inst->Parent == nullptr);
  Inst->Parent = this;
  Instructions.push_back(Inst);
}

void BasicBlock::push_front(pointer Inst) {
  assert(Inst->Parent == nullptr);
  Inst->Parent = this;
  Instructions.push_front(Inst);
}

void BasicBlock::clear() { Instructions.clear(); }

void BasicBlock::pop_back() { Instructions.pop_back(); }

void BasicBlock::pop_front() { Instructions.pop_front(); }

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
// clang-format on

bool BasicBlock::empty() const { return Instructions.empty(); }
BasicBlock::size_type BasicBlock::size() const { return Instructions.size(); }

bool BasicBlock::isEntryBlock() const {
  auto const *ParentFunction = getParent();
  return std::addressof(ParentFunction->getEntryBasicBlock()) == this;
}

bool BasicBlock::hasNoInwardFlow() const {
  bool HasNoInwardFlow = true;
  for (auto const *ASTNode : this->getUsedSites()) {
    if (!is_a<Instruction>(ASTNode)) continue;
    auto *CastedPtr = dyn_cast<Instruction>(ASTNode);
    if (CastedPtr->isBranching()) HasNoInwardFlow = false;
  }
  return HasNoInwardFlow;
}

llvm::SmallPtrSet<BasicBlock *, 4> BasicBlock::getInwardFlow() const {
  llvm::SmallPtrSet<BasicBlock *, 4> Result;
  for (auto const *ASTNode : this->getUsedSites()) {
    if (!is_a<Instruction>(ASTNode)) continue;
    auto *CastedPtr = dyn_cast<Instruction>(ASTNode);
    if (CastedPtr->isBranching()) Result.insert(CastedPtr->getParent());
  }
  return Result;
}

namespace {
struct OutwardFlowVisitor :
    mir::InstVisitorBase<
        OutwardFlowVisitor, llvm::SmallPtrSet<BasicBlock *, 2>> {
  using ResultType = llvm::SmallPtrSet<BasicBlock *, 2>;
  ResultType operator()(instructions::Unreachable const *) { return {}; }
  ResultType operator()(instructions::Return const *) { return {}; }
  ResultType operator()(instructions::Branch const *Inst) {
    ResultType Out;
    if (Inst->getTarget()) Out.insert(Inst->getTarget());
    if (Inst->getFalseTarget()) Out.insert(Inst->getFalseTarget());
    return Out;
  };
  ResultType operator()(instructions::BranchTable const *Inst) {
    ResultType Out;
    if (Inst->getDefaultTarget()) Out.insert(Inst->getDefaultTarget());
    for (auto *Target : Inst->getTargets())
      if (Target) Out.insert(Target);
    return Out;
  }
  template <mir::instruction T> ResultType operator()(T const *) {
    utility::unreachable();
  }
};
} // namespace

llvm::SmallPtrSet<BasicBlock *, 2> BasicBlock::getOutwardFlow() const {
  OutwardFlowVisitor Visitor;
  return Visitor.visit(std::addressof(back()));
}

Instruction *BasicBlock::getTerminatingInst() {
  if (!empty() && !back().isTerminating()) { return std::addressof(back()); }
  return nullptr;
}

void BasicBlock::eraseFromParent() {
  auto &Parent = *getParent();
  Parent.getBasicBlocks().erase(this);
}

// clang-format off
Function *BasicBlock::getParent() const { return Parent; }
llvm::ilist<Instruction> BasicBlock::*
BasicBlock::getSublistAccess(Instruction *) 
{ return &BasicBlock::Instructions; }
void BasicBlock::replace(ASTNode const *, ASTNode *) noexcept 
{ utility::unreachable(); }
bool BasicBlock::classof(ASTNode const *Node) 
{ return Node->getASTNodeKind() == ASTNodeKind::BasicBlock; }
// clang-format on
} // namespace mir