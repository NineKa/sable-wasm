#include "BasicBlock.h"
#include "Binary.h"
#include "Branch.h"
#include "Cast.h"
#include "Compare.h"
#include "Function.h"
#include "Instruction.h"
#include "Module.h"
#include "Unary.h"
#include "Vector.h"

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/unique.hpp>
#include <range/v3/view/filter.hpp>

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
using OutwardFlowSet = llvm::SmallPtrSet<BasicBlock *, 2>;
struct OutwardFlowVisitor :
    mir::InstVisitorBase<OutwardFlowVisitor, OutwardFlowSet>,
    mir::instructions::BranchVisitorBase<OutwardFlowVisitor, OutwardFlowSet> {
  using InstVisitorBase::visit;

  OutwardFlowSet operator()(instructions::Unreachable const *) { return {}; }
  OutwardFlowSet operator()(instructions::Return const *) { return {}; }

  OutwardFlowSet operator()(instructions::branch::Unconditional const *Inst) {
    OutwardFlowSet Out;
    if (Inst->getTarget()) Out.insert(Inst->getTarget());
    return Out;
  }

  OutwardFlowSet operator()(instructions::branch::Conditional const *Inst) {
    OutwardFlowSet Out;
    if (Inst->getTrue()) Out.insert(Inst->getTrue());
    if (Inst->getFalse()) Out.insert(Inst->getFalse());
    return Out;
  }

  OutwardFlowSet operator()(instructions::branch::Switch const *Inst) {
    OutwardFlowSet Out;
    if (Inst->getDefaultTarget()) Out.insert(Inst->getDefaultTarget());
    for (auto *Target : Inst->getTargets())
      if (Target) Out.insert(Target);
    return Out;
  }

  OutwardFlowSet operator()(instructions::Branch const *Inst) {
    return BranchVisitorBase::visit(Inst);
  }

  OutwardFlowSet operator()(Instruction const *) { utility::unreachable(); }
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

void BasicBlock::replaceAllUseWith(BasicBlock *ReplaceValue) const {
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

void BasicBlock::eraseFromParent() { Parent->getBasicBlocks().erase(this); }

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