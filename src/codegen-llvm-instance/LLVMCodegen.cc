#include "LLVMCodege.h"

#include "../mir/passes/Dominator.h"
#include "../mir/passes/Pass.h"
#include "../mir/passes/TypeInfer.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>

#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <cassert>
#include <memory>
#include <unordered_map>

namespace codegen::llvm_instance {

class FunctionTranslationTask::TranslationContext {
  EntityLayout &Layout;
  mir::Function const &Source;
  llvm::Function &Target;

  std::unordered_map<mir::Instruction const *, llvm::Value *> ValueMap;
  std::unordered_map<mir::Local const *, llvm::Value *> LocalMap;
  std::shared_ptr<mir::passes::DominatorTreeNode> DominatorTree;
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
  TranslationContext(
      EntityLayout &Layout_, mir::Function const &Source_,
      llvm::Function &Target_)
      : Layout(Layout_), Source(Source_), Target(Target_) {
    auto const &EntryBB = Source.getEntryBasicBlock();
    mir::passes::SimpleFunctionPassDriver<mir::passes::DominatorPass>
        DominatorPassDriver;
    DominatorTree = DominatorPassDriver(Source).buildDomTree(EntryBB);
    mir::passes::TypeInferPass TypeInferPass;
    TypeInferPass.prepare(Source, DominatorTree);
    TypeInferPass.run();
    TypeInferPass.finalize();
    TypePassResult = TypeInferPass.getResult();

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

  llvm::Value *getAsBool(
      llvm::IRBuilder<> &Builder, mir::Instruction const &Instruction) const {
    assert(getInferredType()[Instruction].isPrimitiveI32());
    auto *Value = this->operator[](Instruction);
    if (Value->getType() == Builder.getInt1Ty()) return Value;
    /*
    assert(Value->getType() == getLayout().getI32Ty());
    return Builder.CreateTrunc(Value, Builder.getInt1Ty());
    */
    if (Value->getType() == getLayout().getI32Ty())
      return Builder.CreateTrunc(Value, Builder.getInt1Ty());
    return Value;
  }

  llvm::Value *getAsValue(
      llvm::IRBuilder<> &Builder, mir::Instruction const &Instruction) const {
    auto *Value = this->operator[](Instruction);
    auto const &InferredType = getInferredType()[Instruction];
    assert(InferredType.isPrimitive() || InferredType.isAggregate());
    if (InferredType.isAggregate()) return Value;
    using VKind = bytecode::ValueTypeKind;
    switch (InferredType.asPrimitive().getKind()) {
    case VKind::I32: {
      if (Value->getType() == getLayout().getI32Ty()) return Value;
      /* assert(Value->getType() == Builder.getInt1Ty());
      return Builder.CreateZExt(Value, getLayout().getI32Ty());
       */
      if (Value->getType() == Builder.getInt1Ty())
        return Builder.CreateZExt(Value, getLayout().getI32Ty());
      return Value;
    }
    default: return Value;
    }
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

  EntityLayout const &getLayout() const { return Layout; }

  std::shared_ptr<mir::passes::DominatorTreeNode> getDominatorTree() const {
    return DominatorTree;
  }

  mir::passes::TypeInferPassResult const &getInferredType() const {
    return TypePassResult;
  }

  mir::Function const &getSource() const { return Source; }
  llvm::Function &getTarget() { return Target; }
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
      auto *Condition = Context.getAsBool(Builder, *Inst->getCondition());
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

  llvm::Value *operator()(minsts::BranchTable const *Inst) {
    auto *Operand = Context.getAsValue(Builder, *Inst->getOperand());
    auto [DefaultFirst, DefaultLast] = Context[*Inst->getDefaultTarget()];
    utility::ignore(DefaultLast);
    // clang-format off
    auto Targets = Inst->getTargets()
      | ranges::views::transform([&](mir::BasicBlock const *Target) {
          auto [TargetFirst, TargetLast] = Context[*Target];
          utility::ignore(TargetLast);
          return TargetFirst;
        })
      | ranges::to<std::vector<llvm::BasicBlock *>>();
    // clang-format on
    auto *LLVMSwitch =
        Builder.CreateSwitch(Operand, DefaultFirst, Targets.size());
    for (auto const &[Index, Target] : ranges::views::enumerate(Targets))
      LLVMSwitch->addCase(Context.getLayout().getI32Constant(Index), Target);
    return LLVMSwitch;
  }

  llvm::Value *operator()(minsts::Return const *Inst) {
    if (Inst->hasReturnValue()) {
      auto *ReturnValue = Context.getAsValue(Builder, *Inst->getOperand());
      return Builder.CreateRet(ReturnValue);
    }
    return Builder.CreateRetVoid();
  }

  llvm::Value *operator()(minsts::Call const *Inst) {
    auto *InstancePtr = Context.getTarget().arg_begin();
    auto *Target = Context.getLayout()[*Inst->getTarget()].definition();
    std::vector<llvm::Value *> Arguments;
    Arguments.reserve(Inst->getTarget()->getType().getNumParameter() + 1);
    Arguments.push_back(InstancePtr);
    for (auto const *Argument : Inst->getArguments())
      Arguments.push_back(Context.getAsValue(Builder, *Argument));
    return Builder.CreateCall(Target, Arguments);
  }

  llvm::Value *operator()(minsts::CallIndirect const *Inst) {
    auto *InstancePtr = Context.getTarget().arg_begin();
    auto *Index = Context.getAsValue(Builder, *Inst->getOperand());
    auto *Table = Context.getLayout().get(
        Builder, InstancePtr, *Inst->getIndirectTable());
    std::vector<llvm::Value *> Arguments;
    Arguments.reserve(Inst->getNumArguments() + 1);
    Arguments.push_back(InstancePtr);
    for (auto const *Argument : Inst->getArguments())
      Arguments.push_back(Context.getAsValue(Builder, *Argument));
    auto *BuiltinTableGuard =
        Context.getLayout().getBuiltin("__sable_table_guard");
    Builder.CreateCall(BuiltinTableGuard, {Table, Index});
    auto *BuiltinTableGet = Context.getLayout().getBuiltin("__sable_table_get");
    auto *TypeString = Context.getLayout().getCStringPtr(
        Context.getLayout().getTypeString(Inst->getExpectType()),
        "typestr.indirect.call");
    auto *TargetType = Context.getLayout().convertType(Inst->getExpectType());
    auto *TargetPtrType = llvm::PointerType::getUnqual(TargetType);
    llvm::Value *Target =
        Builder.CreateCall(BuiltinTableGet, {Table, Index, TypeString});
    Target = Builder.CreatePointerCast(Target, TargetPtrType);
    llvm::FunctionCallee Callee(TargetType, Target);
    return Builder.CreateCall(Callee, Arguments);
  }

  llvm::Value *operator()(minsts::Select const *Inst) {
    auto *Condition = Context.getAsBool(Builder, *Inst->getCondition());
    auto *True = Context.getAsValue(Builder, *Inst->getTrue());
    auto *False = Context.getAsValue(Builder, *Inst->getFalse());
    return Builder.CreateSelect(Condition, True, False);
  }

  llvm::Value *operator()(minsts::LocalGet const *Inst) {
    auto *Local = Context[*Inst->getTarget()];
    return Builder.CreateLoad(Local);
  }

  llvm::Value *operator()(minsts::LocalSet const *Inst) {
    auto *Local = Context[*Inst->getTarget()];
    auto *Value = Context.getAsValue(Builder, *Inst->getOperand());
    return Builder.CreateStore(Value, Local);
  }

  llvm::Value *operator()(minsts::Constant const *Inst) {
    using VKind = bytecode::ValueTypeKind;
    switch (Inst->getValueType().getKind()) {
    case VKind::I32: return Context.getLayout().getI32Constant(Inst->asI32());
    case VKind::I64: return Context.getLayout().getI64Constant(Inst->asI64());
    case VKind::F32: return Context.getLayout().getF32Constant(Inst->asF32());
    case VKind::F64: return Context.getLayout().getF64Constant(Inst->asF64());
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
FunctionTranslationTask &FunctionTranslationTask::operator=(
    FunctionTranslationTask &&) noexcept = default;
FunctionTranslationTask::~FunctionTranslationTask() noexcept = default;

void FunctionTranslationTask::perform() {
  for (auto const *BBPtr : Context->getDominatorTree()->asPreorder()) {
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
