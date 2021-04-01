#include "LLVMCodege.h"

#include "../mir/passes/Dominator.h"
#include "../mir/passes/Pass.h"
#include "../mir/passes/TypeInfer.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>

#include <cassert>
#include <memory>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>

#include <unordered_map>

namespace codegen::llvm_instance {

class FunctionTranslationTask::TranslationContext {
  EntityLayout &Layout;
  mir::Function const &Source;
  llvm::Function &Target;

  std::unordered_map<mir::Instruction const *, llvm::Value *> ValueMap;
  std::unordered_map<mir::Local const *, llvm::Value *> LocalMap;
  mir::passes::DominatorPassResult DomPassResult;
  mir::passes::TypeInferPassResult TypePassResult;

  std::unordered_map<
      mir::BasicBlock const *,
      std::pair<llvm::BasicBlock *, llvm::BasicBlock *>>
      BasicBlockMap;

  llvm::Value *getLocalInitializer(std::size_t Index, mir::Local const &Local) {
    if (Local.isParameter()) return Target.arg_begin() + (Index + 1);
    using VKind = bytecode::ValueTypeKind;
    switch (Local.getType().getKind()) {
    case VKind::I32: return Layout.getI32Constant(0);
    case VKind::I64: return Layout.getI64Constant(0);
    case VKind::F32: return Layout.getF32Constant(0);
    case VKind::F64: return Layout.getF64Constant(0);
    default: utility::unreachable();
    }
  }

public:
  using DominatorPassDriver =
      mir::passes::SimpleFunctionPassDriver<mir::passes::DominatorPass>;
  using TypeInferPassDriver =
      mir::passes::SimpleFunctionPassDriver<mir::passes::TypeInferPass>;
  TranslationContext(
      EntityLayout &Layout_, mir::Function const &Source_,
      llvm::Function &Target_)
      : Layout(Layout_), Source(Source_), Target(Target_),
        DomPassResult(DominatorPassDriver()(Source)),
        TypePassResult(TypeInferPassDriver()(Source)) {
    auto *EntryBlock = llvm::BasicBlock::Create(
        /* Context */ Target.getContext(),
        /* Name    */ "pre_entry",
        /* Parent  */ std::addressof(Target));
    llvm::IRBuilder<> Builder(EntryBlock);
    for (auto const &[Index, Local] :
         ranges::views::enumerate(Source.getLocals().asView())) {
      auto *LLVMLocal =
          Builder.CreateAlloca(Layout.convertType(Local.getType()));
      LocalMap.emplace(std::addressof(Local), LLVMLocal);
      Builder.CreateStore(getLocalInitializer(Index, Local), LLVMLocal);
    }

    for (auto const &BasicBlock : Source.getBasicBlocks().asView()) {
      auto *BB = llvm::BasicBlock::Create(
          /* Context */ Target.getContext(),
          /* Name    */ llvm::StringRef(BasicBlock.getName()),
          /* Parent  */ std::addressof(Target));
      auto Entry = std::make_pair(BB, BB);
      BasicBlockMap.emplace(std::addressof(BasicBlock), Entry);
    }

    auto *FirstBB = std::addressof(Source.getEntryBasicBlock());
    auto *FirstLLVMBB = std::get<0>(std::get<1>(*BasicBlockMap.find(FirstBB)));
    Builder.CreateBr(FirstLLVMBB);
  }

  llvm::Value *operator[](mir::Instruction const &Instruction) const {
    auto SearchIter = ValueMap.find(std::addressof(Instruction));
    assert(SearchIter != ValueMap.end());
    return std::get<1>(*SearchIter);
  }

  llvm::Value *operator[](mir::Local const &Local) const {
    auto SearchIter = LocalMap.find(std::addressof(Local));
    assert(SearchIter != LocalMap.end());
    return std::get<1>(*SearchIter);
  }

  std::pair<llvm::BasicBlock *, llvm::BasicBlock *>
  operator[](mir::BasicBlock const &BasicBlock) const {
    auto SearchIter = BasicBlockMap.find(std::addressof(BasicBlock));
    assert(SearchIter != BasicBlockMap.end());
    return std::get<1>(*SearchIter);
  }

  llvm::BasicBlock *createBasicBlock(mir::BasicBlock const &BasicBlock) {
    llvm::BasicBlock *InsertPos = nullptr;
    if (std::next(BasicBlock.getIterator()) != Source.getBasicBlocks().end()) {
      auto const &NextBB = *std::next(BasicBlock.getIterator());
      auto [LLVMNextBB, LLVMLastBB] = this->operator[](NextBB);
      utility::ignore(LLVMLastBB);
      InsertPos = LLVMNextBB;
    }
    auto [FirstBB, LastBB] = this->operator[](BasicBlock);
    LastBB = llvm::BasicBlock::Create(
        /* Context       */ Target.getContext(),
        /* Name          */ "",
        /* Parent        */ std::addressof(Target),
        /* Insert Before */ InsertPos);
    auto NewEntry = std::make_pair(FirstBB, LastBB);
    BasicBlockMap.emplace(std::addressof(BasicBlock), NewEntry);
    return LastBB;
  }

  void setValueMapping(mir::Instruction const &Inst, llvm::Value *Value) {
    ValueMap.emplace(std::addressof(Inst), Value);
  }

  EntityLayout &layout() { return Layout; }

  mir::passes::DominatorPassResult const &dominator() const {
    return DomPassResult;
  }

  mir::passes::TypeInferPassResult const &type() const {
    return TypePassResult;
  }

  mir::Function const &source() const { return Source; }
  llvm::Function &target() { return Target; }
};

namespace minsts = mir::instructions;
class FunctionTranslationTask::TranslationVisitor :
    public mir::InstVisitorBase<TranslationVisitor, llvm::Value *> {
  FunctionTranslationTask::TranslationContext &Context;
  llvm::IRBuilder<> &Builder;

public:
  explicit TranslationVisitor(
      TranslationContext &Context_, llvm::IRBuilder<> &Builder_)
      : Context(Context_), Builder(Builder_) {}

  llvm::Value *operator()(minsts::Unreachable const *) {
    return Builder.CreateUnreachable();
  }

  llvm::Value *operator()(minsts::Branch const *Inst) {
    llvm::Value *LLVMBrInstruction = nullptr;
    if (Inst->isConditional()) {
      auto *Condition = Context[*Inst->getCondition()];
      auto *TrueBB = std::get<0>(Context[*Inst->getTarget()]);
      auto *FalseBB = std::get<0>(Context[*Inst->getFalseTarget()]);
      LLVMBrInstruction = Builder.CreateCondBr(Condition, TrueBB, FalseBB);
    } else /* Inst->isUnconditional() */ {
      assert(Inst->isUnconditional());
      auto *TargetBB = std::get<0>(Context[*Inst->getTarget()]);
      LLVMBrInstruction = Builder.CreateBr(TargetBB);
    }
    return LLVMBrInstruction;
  }

  llvm::Value *operator()(minsts::Return const *Inst) {
    if (Inst->hasReturnValue()) {
      auto *ReturnValue = Context[*Inst->getOperand()];
      return Builder.CreateRet(ReturnValue);
    }
    return Builder.CreateRetVoid();
  }

  llvm::Value *operator()(minsts::Constant const *Inst) {
    using VKind = bytecode::ValueTypeKind;
    switch (Inst->getValueType().getKind()) {
    case VKind::I32: return Context.layout().getI32Constant(Inst->asI32());
    case VKind::I64: return Context.layout().getI64Constant(Inst->asI64());
    case VKind::F32: return Context.layout().getF32Constant(Inst->asF32());
    case VKind::F64: return Context.layout().getF64Constant(Inst->asF64());
    default: utility::unreachable();
    }
  }

  template <mir::instruction T> llvm::Value *operator()(T const *) {
    utility::ignore(Context);
    return Builder.CreateUnreachable();
  }
};

FunctionTranslationTask::FunctionTranslationTask(
    EntityLayout &EntityLayout_, mir::Function const &Source_,
    llvm::Function &Target_)
    : Context(std::make_unique<TranslationContext>(
          EntityLayout_, Source_, Target_)) {}

FunctionTranslationTask::FunctionTranslationTask(
    FunctionTranslationTask &&) noexcept = default;
FunctionTranslationTask::~FunctionTranslationTask() noexcept = default;

void FunctionTranslationTask::perform() {
  auto const &EntryBB = Context->source().getEntryBasicBlock();
  auto DomTree = Context->dominator().buildDomTree(EntryBB);
  for (auto const *BBPtr : DomTree->asPreorder()) {
    auto [FirstBBPtr, LastBBPtr] = Context->operator[](*BBPtr);
    utility::ignore(LastBBPtr);
    llvm::IRBuilder<> Builder(FirstBBPtr);
    TranslationVisitor Visitor(*Context, Builder);
    for (auto const &Instruction : *BBPtr) {
      auto *Value = Visitor.visit(std::addressof(Instruction));
      Context->setValueMapping(Instruction, Value);
    }
  }
}

ModuleTranslationTask::ModuleTranslationTask(
    mir::Module const &Source_, llvm::Module &Target_)
    : Layout(nullptr), Source(std::addressof(Source_)),
      Target(std::addressof(Target_)) {}

void ModuleTranslationTask::perform() {
  Layout = std::make_unique<EntityLayout>(*Source, *Target);
  for (auto const &Function : Source->getFunctions().asView()) {
    if (Function.isDeclaration()) continue;
    auto &TargetFunction = *Layout->operator[](Function).definition();
    FunctionTranslationTask Task(*Layout, Function, TargetFunction);
    Task.perform();
  }
}

} // namespace codegen::llvm_instance
