#include "LLVMCodege.h"

#include "../mir/passes/Dominator.h"
#include "../mir/passes/Pass.h"
#include "../mir/passes/TypeInfer.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
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
#ifndef NDEBUG
    auto *Value = std::get<1>(*SearchIter);
    auto ExpectType = TypePassResult[Instruction];
    assert(ExpectType.isPrimitive() || ExpectType.isAggregate());
    if (ExpectType.isPrimitive())
      assert(Layout.convertType(ExpectType.asPrimitive()) == Value->getType());
    if (ExpectType.isAggregate()) {
      // clang-format off
      auto Members = ExpectType.asAggregate()
        | ranges::views::transform([&](bytecode::ValueType const &ValueType) {
            return Layout.convertType(ValueType);
          })
        | ranges::to<std::vector<llvm::Type *>>();
      // clang-format on
      auto *StructTy = llvm::StructType::get(Target.getContext(), Members);
      auto *StructPtrTy = llvm::PointerType::getUnqual(StructTy);
      assert(StructPtrTy == Value->getType());
    }
#endif
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

  EntityLayout const &getLayout() const { return Layout; }

  std::shared_ptr<mir::passes::DominatorTreeNode> getDominatorTree() const {
    return DominatorTree;
  }

  mir::passes::TypeInferPassResult const &getInferredType() const {
    return TypePassResult;
  }

  mir::Function const &getSource() const { return Source; }
  llvm::Function &getTarget() { return Target; }
  llvm::Argument *getInstancePtr() const { return Target.arg_begin(); }
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
    auto *BuiltinUnreachable =
        Context.getLayout().getBuiltin("__sable_unreachable");
    Builder.CreateCall(BuiltinUnreachable);
    return Builder.CreateUnreachable();
  }

  llvm::Value *operator()(minsts::Branch const *Inst) {
    llvm::Value *LLVMBrInstruction = nullptr;
    if (Inst->isConditional()) {
      llvm::Value *Condition = Context[*Inst->getCondition()];
      assert(Context.getInferredType()[*Inst->getCondition()].isPrimitiveI32());
      assert(Condition->getType() == Context.getLayout().getI32Ty());
      auto *Zero = Context.getLayout().getI32Constant(0);
      Condition = Builder.CreateICmpNE(Condition, Zero);
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
    auto *Operand = Context[*Inst->getOperand()];
    assert(Context.getInferredType()[*Inst->getOperand()].isPrimitiveI32());
    assert(Operand->getType() == Context.getLayout().getI32Ty());
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
      auto *ReturnValue = Context[*Inst->getOperand()];
      auto ReturnTy = Context.getInferredType()[*Inst->getOperand()];
      return Builder.CreateRet(ReturnValue);
    }
    return Builder.CreateRetVoid();
  }

  llvm::Value *operator()(minsts::Call const *Inst) {
    auto *InstancePtr = Context.getInstancePtr();
    auto *Callee = Context.getLayout()[*Inst->getTarget()].definition();
    std::vector<llvm::Value *> Arguments;
    Arguments.reserve(Inst->getTarget()->getType().getNumParameter() + 1);
    Arguments.push_back(InstancePtr);
    for (auto const *Argument : Inst->getArguments())
      Arguments.push_back(Context[*Argument]);
    return Builder.CreateCall(Callee, Arguments);
  }

  llvm::Value *operator()(minsts::CallIndirect const *Inst) {
    auto *InstancePtr = Context.getInstancePtr();
    auto *Index = Context[*Inst->getOperand()];
    auto *Table = Context.getLayout().get(
        Builder, InstancePtr, *Inst->getIndirectTable());
    auto *BuiltinTableGuard =
        Context.getLayout().getBuiltin("__sable_table_guard");
    Builder.CreateCall(BuiltinTableGuard, {Table, Index});
    auto *TypeString = Context.getLayout().getCStringPtr(
        Context.getLayout().getTypeString(Inst->getExpectType()),
        "typestr.indirect.call");
    auto *BuiltinTableCheck =
        Context.getLayout().getBuiltin("__sable_table_check");
    Builder.CreateCall(BuiltinTableCheck, {Table, Index, TypeString});
    auto *BuiltinTableFunctionPtr =
        Context.getLayout().getBuiltin("__sable_table_function_ptr");
    auto *BuiltinTableInstanceClosurePtr =
        Context.getLayout().getBuiltin("__sable_table_instance_closure");

    llvm::Value *CalleeInstanceClosure =
        Builder.CreateCall(BuiltinTableInstanceClosurePtr, {Table, Index});
    auto *CalleeFunctionPtr =
        Builder.CreateCall(BuiltinTableFunctionPtr, {Table, Index});

    auto *Null = llvm::ConstantInt::get(Context.getLayout().getPtrIntTy(), 0);
    llvm::Value *IsNullTest = Builder.CreatePtrToInt(
        CalleeInstanceClosure, Context.getLayout().getPtrIntTy());
    IsNullTest = Builder.CreateICmpEQ(IsNullTest, Null);
    CalleeInstanceClosure =
        Builder.CreateSelect(IsNullTest, InstancePtr, CalleeInstanceClosure);

    std::vector<llvm::Value *> Arguments;
    Arguments.reserve(Inst->getNumArguments() + 1);
    Arguments.push_back(CalleeInstanceClosure);
    for (auto const *Argument : Inst->getArguments())
      Arguments.push_back(Context[*Argument]);

    auto *CalleeTy = Context.getLayout().convertType(Inst->getExpectType());
    auto *CalleePtrTy = llvm::PointerType::getUnqual(CalleeTy);
    auto *CalleePtr = Builder.CreatePointerCast(CalleeFunctionPtr, CalleePtrTy);
    llvm::FunctionCallee Callee(CalleeTy, CalleePtr);
    return Builder.CreateCall(Callee, Arguments);
  }

  llvm::Value *operator()(minsts::Select const *Inst) {
    llvm::Value *Condition = Context[*Inst->getCondition()];
    assert(Context.getInferredType()[*Inst->getCondition()].isPrimitiveI32());
    assert(Condition->getType() == Context.getLayout().getI32Ty());
    auto *Zero = Context.getLayout().getI32Constant(0);
    Condition = Builder.CreateICmpNE(Condition, Zero);
    auto *True = Context[*Inst->getTrue()];
    auto *False = Context[*Inst->getFalse()];
    return Builder.CreateSelect(Condition, True, False);
  }

  llvm::Value *operator()(minsts::LocalGet const *Inst) {
    auto *Local = Context[*Inst->getTarget()];
    return Builder.CreateLoad(Local);
  }

  llvm::Value *operator()(minsts::LocalSet const *Inst) {
    auto *Local = Context[*Inst->getTarget()];
    auto *Value = Context[*Inst->getOperand()];
    return Builder.CreateStore(Value, Local);
  }

  llvm::Value *operator()(minsts::GlobalGet const *Inst) {
    auto *InstancePtr = Context.getInstancePtr();
    llvm::Value *Global =
        Context.getLayout().get(Builder, InstancePtr, *Inst->getTarget());
    auto GlobalValueType = Inst->getTarget()->getType().getType();
    auto *GlobalType = Context.getLayout().convertType(GlobalValueType);
    auto *GlobalPtrType = llvm::PointerType::getUnqual(GlobalType);
    Global = Builder.CreatePointerCast(Global, GlobalPtrType);
    return Builder.CreateLoad(Global);
  }

  llvm::Value *operator()(minsts::GlobalSet const *Inst) {
    auto *InstancePtr = Context.getInstancePtr();
    auto *Value = Context[*Inst->getOperand()];
    llvm::Value *Global =
        Context.getLayout().get(Builder, InstancePtr, *Inst->getTarget());
    auto GlobalValueType = Inst->getTarget()->getType().getType();
    auto *GlobalType = Context.getLayout().convertType(GlobalValueType);
    auto *GlobalPtrType = llvm::PointerType::getUnqual(GlobalType);
    Global = Builder.CreatePointerCast(Global, GlobalPtrType);
    return Builder.CreateStore(Value, Global);
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

  template <llvm::Intrinsic::ID IntrinsicID, typename... ArgTypes>
  llvm::Function *getIntrinsic(ArgTypes &&...Args) {
    auto *EnclosingModule = Context.getTarget().getParent();
    std::array<llvm::Type *, sizeof...(ArgTypes)> IntrinsicArgumentTys{
        std::forward<ArgTypes>(Args)...};
    return llvm::Intrinsic::getDeclaration(
        EnclosingModule, IntrinsicID, IntrinsicArgumentTys);
  }

  llvm::Value *operator()(minsts::IntUnaryOp const *Inst) {
    auto *MIROperand = Inst->getOperand();
    auto *Operand = Context[*MIROperand];
    switch (Inst->getOperator()) {
    case minsts::IntUnaryOperator::Eqz: {
      llvm::Value *Zero = nullptr;
      if (Context.getInferredType()[*MIROperand].isPrimitiveI32()) {
        Zero = Context.getLayout().getI32Constant(0);
      } else /* Context.getInferredType()[*MIROperand].isPrimitiveI64() */ {
        assert(Context.getInferredType()[*MIROperand].isPrimitiveI64());
        Zero = Context.getLayout().getI64Constant(0);
      }
      return Builder.CreateICmpEQ(Operand, Zero);
    }
    case minsts::IntUnaryOperator::Clz: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::ctlz>(
          Context.getLayout().convertType(OperandType.asPrimitive()),
          Builder.getInt1Ty());
      return Builder.CreateCall(Intrinsic, {Operand, Builder.getFalse()});
    }
    case minsts::IntUnaryOperator::Ctz: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::cttz>(
          Context.getLayout().convertType(OperandType.asPrimitive()),
          Builder.getInt1Ty());
      return Builder.CreateCall(Intrinsic, {Operand, Builder.getFalse()});
    }
    case minsts::IntUnaryOperator::Popcnt: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::ctpop>(
          Context.getLayout().convertType(OperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {Operand});
    }
    default: utility::unreachable();
    }
  }

  llvm::Value *operator()(minsts::IntBinaryOp const *Inst) {
    auto *MIRValueLHS = Inst->getLHS();
    auto *MIRValueRHS = Inst->getRHS();
    auto *LHS = Context[*MIRValueLHS];
    auto *RHS = Context[*MIRValueRHS];
    using IntBinaryOperator = mir::instructions::IntBinaryOperator;
    switch (Inst->getOperator()) {
    case IntBinaryOperator::Eq: return Builder.CreateICmpEQ(LHS, RHS);
    case IntBinaryOperator::Ne: return Builder.CreateICmpNE(LHS, RHS);
    case IntBinaryOperator::LtS: return Builder.CreateICmpSLT(LHS, RHS);
    case IntBinaryOperator::LtU: return Builder.CreateICmpULT(LHS, RHS);
    case IntBinaryOperator::GtS: return Builder.CreateICmpSGT(LHS, RHS);
    case IntBinaryOperator::GtU: return Builder.CreateICmpUGT(LHS, RHS);
    case IntBinaryOperator::LeS: return Builder.CreateICmpSLE(LHS, RHS);
    case IntBinaryOperator::LeU: return Builder.CreateICmpULE(LHS, RHS);
    case IntBinaryOperator::GeS: return Builder.CreateICmpSGE(LHS, RHS);
    case IntBinaryOperator::GeU: return Builder.CreateICmpUGE(LHS, RHS);
    case IntBinaryOperator::Add: return Builder.CreateAdd(LHS, RHS);
    case IntBinaryOperator::Sub: return Builder.CreateSub(LHS, RHS);
    case IntBinaryOperator::Mul: return Builder.CreateMul(LHS, RHS);
    case IntBinaryOperator::DivS: return Builder.CreateSDiv(LHS, RHS);
    case IntBinaryOperator::DivU: return Builder.CreateUDiv(LHS, RHS);
    case IntBinaryOperator::RemS: return Builder.CreateSRem(LHS, RHS);
    case IntBinaryOperator::RemU: return Builder.CreateURem(LHS, RHS);
    case IntBinaryOperator::And: return Builder.CreateAnd(LHS, RHS);
    case IntBinaryOperator::Or: return Builder.CreateOr(LHS, RHS);
    case IntBinaryOperator::Xor: return Builder.CreateXor(LHS, RHS);
    case IntBinaryOperator::Shl: return Builder.CreateShl(LHS, RHS);
    case IntBinaryOperator::ShrS: return Builder.CreateAShr(LHS, RHS);
    case IntBinaryOperator::ShrU: return Builder.CreateLShr(LHS, RHS);
    case IntBinaryOperator::Rotl: {
      auto OperandType = Context.getInferredType()[*MIRValueLHS];
      auto ShiftAmountType = Context.getInferredType()[*MIRValueRHS];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::fshl>(
          Context.getLayout().convertType(OperandType.asPrimitive()),
          Context.getLayout().convertType(OperandType.asPrimitive()),
          Context.getLayout().convertType(ShiftAmountType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {LHS, LHS, RHS});
    }
    case IntBinaryOperator::Rotr: {
      auto OperandType = Context.getInferredType()[*MIRValueLHS];
      auto ShiftAmountType = Context.getInferredType()[*MIRValueRHS];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::fshr>(
          Context.getLayout().convertType(OperandType.asPrimitive()),
          Context.getLayout().convertType(OperandType.asPrimitive()),
          Context.getLayout().convertType(ShiftAmountType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {LHS, LHS, RHS});
    }
    default: utility::unreachable();
    }
  }

  llvm::Value *operator()(minsts::FPUnaryOp const *Inst) {
    auto *MIROperand = Inst->getOperand();
    auto *Operand = Context[*MIROperand];
    using FPUnaryOperator = mir::instructions::FPUnaryOperator;
    switch (Inst->getOperator()) {
    case FPUnaryOperator::Abs: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::fabs>(
          Context.getLayout().convertType(OperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {Operand});
    }
    case FPUnaryOperator::Neg: return Builder.CreateFNeg(Operand);
    case FPUnaryOperator::Ceil: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::ceil>(
          Context.getLayout().convertType(OperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {Operand});
    }
    case FPUnaryOperator::Floor: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::floor>(
          Context.getLayout().convertType(OperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {Operand});
    }
    case FPUnaryOperator::Trunc: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::trunc>(
          Context.getLayout().convertType(OperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {Operand});
    }
    case FPUnaryOperator::Nearest: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::nearbyint>(
          Context.getLayout().convertType(OperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {Operand});
    }
    case FPUnaryOperator::Sqrt: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::sqrt>(
          Context.getLayout().convertType(OperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {Operand});
    }
    default: utility::unreachable();
    }
  }

  llvm::Value *operator()(minsts::FPBinaryOp const *Inst) {
    auto *MIRValueLHS = Inst->getLHS();
    auto *MIRValueRHS = Inst->getRHS();
    auto *LHS = Context[*MIRValueLHS];
    auto *RHS = Context[*MIRValueRHS];
    using FPBinaryOperator = mir::instructions::FPBinaryOperator;
    switch (Inst->getOperator()) {
    case FPBinaryOperator::Eq: return Builder.CreateFCmpOEQ(LHS, RHS);
    case FPBinaryOperator::Ne: return Builder.CreateFCmpONE(LHS, RHS);
    case FPBinaryOperator::Lt: return Builder.CreateFCmpOLT(LHS, RHS);
    case FPBinaryOperator::Gt: return Builder.CreateFCmpOGT(LHS, RHS);
    case FPBinaryOperator::Le: return Builder.CreateFCmpOLE(LHS, RHS);
    case FPBinaryOperator::Ge: return Builder.CreateFCmpOGE(LHS, RHS);
    case FPBinaryOperator::Add: return Builder.CreateFAdd(LHS, RHS);
    case FPBinaryOperator::Sub: return Builder.CreateFSub(LHS, RHS);
    case FPBinaryOperator::Mul: return Builder.CreateFMul(LHS, RHS);
    case FPBinaryOperator::Div: return Builder.CreateFDiv(LHS, RHS);
    case FPBinaryOperator::Min: {
      auto LHSOperandType = Context.getInferredType()[*MIRValueLHS];
      auto RHSOperandType = Context.getInferredType()[*MIRValueRHS];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::minimum>(
          Context.getLayout().convertType(LHSOperandType.asPrimitive()),
          Context.getLayout().convertType(RHSOperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {LHS, RHS});
    }
    case FPBinaryOperator::Max: {
      auto LHSOperandType = Context.getInferredType()[*MIRValueLHS];
      auto RHSOperandType = Context.getInferredType()[*MIRValueRHS];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::maximum>(
          Context.getLayout().convertType(LHSOperandType.asPrimitive()),
          Context.getLayout().convertType(RHSOperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {LHS, RHS});
    }
    case FPBinaryOperator::CopySign: {
      auto LHSOperandType = Context.getInferredType()[*MIRValueLHS];
      auto RHSOperandType = Context.getInferredType()[*MIRValueRHS];
      auto *Intrinsic = getIntrinsic<llvm::Intrinsic::copysign>(
          Context.getLayout().convertType(LHSOperandType.asPrimitive()),
          Context.getLayout().convertType(RHSOperandType.asPrimitive()));
      return Builder.CreateCall(Intrinsic, {LHS, RHS});
    }
    default: utility::unreachable();
    }
  }

  llvm::Value *
  getMemoryLocation(mir::Memory const &Memory, llvm::Value *Offset) {
    auto *InstancePtr = Context.getInstancePtr();
    auto *PtrIntTy = Context.getLayout().getPtrIntTy();
    llvm::Value *Address =
        Context.getLayout().get(Builder, InstancePtr, Memory);
    Address = Builder.CreatePtrToInt(Address, PtrIntTy);
    if (Offset->getType() != PtrIntTy)
      Offset = Builder.CreateZExtOrTrunc(Offset, PtrIntTy);
    Address = Builder.CreateNUWAdd(Address, Offset);
    return Address;
  }

  llvm::Value *operator()(minsts::Load const *Inst) {
    auto *MIRMemory = Inst->getLinearMemory();
    auto *Offset = Context[*Inst->getAddress()];
    auto *Address = getMemoryLocation(*MIRMemory, Offset);
    llvm::Value *Result = nullptr;
    if (Inst->getType().isF32() || Inst->getType().isF64()) {
      auto *LoadTy = Context.getLayout().convertType(Inst->getType());
      auto *LoadPtrTy = llvm::PointerType::getUnqual(LoadTy);
      Address = Builder.CreateIntToPtr(Address, LoadPtrTy);
      auto *LoadInst = Builder.CreateLoad(Address);
      LoadInst->setAlignment(llvm::Align(1));
      Result = LoadInst;
    } else {
      assert(Inst->getType().isI32() || Inst->getType().isI64());
      auto *ExpectLoadTy = llvm::dyn_cast<llvm::IntegerType>(
          Context.getLayout().convertType(Inst->getType()));
      auto *LoadTy = llvm::IntegerType::get(
          Context.getTarget().getContext(), Inst->getLoadWidth());
      auto *LoadPtrTy = llvm::PointerType::getUnqual(LoadTy);
      Address = Builder.CreateIntToPtr(Address, LoadPtrTy);
      auto *LoadInst = Builder.CreateLoad(Address);
      LoadInst->setAlignment(llvm::Align(1));
      Result = LoadInst;
      if (ExpectLoadTy != LoadTy)
        Result = Builder.CreateZExt(Result, ExpectLoadTy);
    }
    return Result;
  }

  llvm::Value *operator()(minsts::Store const *Inst) {
    llvm::Value *Value = Context[*Inst->getOperand()];
    if (Context.getInferredType()[*Inst->getOperand()].isIntegral()) {
      assert(Value->getType()->isIntegerTy());
      auto *CastedTy = llvm::dyn_cast<llvm::IntegerType>(Value->getType());
      assert(Inst->getStoreWidth() <= CastedTy->getIntegerBitWidth());
      if (Inst->getStoreWidth() < CastedTy->getIntegerBitWidth()) {
        auto *TruncatedTy = llvm::IntegerType::getIntNTy(
            Context.getTarget().getContext(), Inst->getStoreWidth());
        Value = Builder.CreateTrunc(Value, TruncatedTy);
      }
    }
    auto *Offset = Context[*Inst->getAddress()];
    auto *Address = getMemoryLocation(*Inst->getLinearMemory(), Offset);
    auto *StoreTy = Value->getType();
    auto *StorePtrTy = llvm::PointerType::getUnqual(StoreTy);
    Address = Builder.CreateIntToPtr(Address, StorePtrTy);
    auto *Result = Builder.CreateStore(Value, Address);
    Result->setAlignment(llvm::Align(1));
    return Result;
  }

  llvm::Value *operator()(minsts::MemoryGuard const *Inst) {
    auto *InstancePtr = Context.getInstancePtr();
    auto *BuiltinMemoryGuard =
        Context.getLayout().getBuiltin("__sable_memory_guard");
    auto *Memory =
        Context.getLayout().get(Builder, InstancePtr, *Inst->getLinearMemory());
    llvm::Value *Offset = Context[*Inst->getAddress()];
    auto *GuardSize = Context.getLayout().getI32Constant(Inst->getGuardSize());
    Offset = Builder.CreateNUWAdd(Offset, GuardSize);
    return Builder.CreateCall(BuiltinMemoryGuard, {Memory, Offset});
  }

  llvm::Value *operator()(minsts::MemoryGrow const *Inst) {
    auto *InstancePtr = Context.getInstancePtr();
    auto *BuiltinMemoryGrow =
        Context.getLayout().getBuiltin("__sable_memory_grow");
    auto *Memory =
        Context.getLayout().get(Builder, InstancePtr, *Inst->getLinearMemory());
    auto *DeltaSize = Context[*Inst->getSize()];
    return Builder.CreateCall(BuiltinMemoryGrow, {Memory, DeltaSize});
  }

  llvm::Value *operator()(minsts::MemorySize const *Inst) {
    auto *InstancePtr = Context.getInstancePtr();
    auto *BuiltinMemorySize =
        Context.getLayout().getBuiltin("__sable_memory_size");
    auto *Memory =
        Context.getLayout().get(Builder, InstancePtr, *Inst->getLinearMemory());
    return Builder.CreateCall(BuiltinMemorySize, {Memory});
  }

  llvm::Value *operator()(minsts::Cast const *Inst) {
    auto *Operand = Context[*Inst->getOperand()];
    auto FromMIRTy = Context.getInferredType()[*Inst->getOperand()];
    auto ToMIRTy = Context.getInferredType()[*Inst];
    auto *ToTy = Context.getLayout().convertType(ToMIRTy.asPrimitive());
    using CastMode = mir::instructions::CastMode;
    switch (Inst->getMode()) {
    case CastMode::Conversion: {
      if (FromMIRTy.isIntegral() && ToMIRTy.isIntegral())
        return Builder.CreateTrunc(Operand, ToTy);
      if (FromMIRTy.isFloatingPoint() && ToMIRTy.isFloatingPoint())
        return Builder.CreateFPCast(Operand, ToTy);
      utility::unreachable();
    }
    case CastMode::ConversionSigned: {
      if (FromMIRTy.isIntegral() && ToMIRTy.isIntegral())
        return Builder.CreateSExt(Operand, ToTy);
      if (FromMIRTy.isIntegral() && ToMIRTy.isFloatingPoint())
        return Builder.CreateSIToFP(Operand, ToTy);
      if (FromMIRTy.isFloatingPoint() && ToMIRTy.isIntegral())
        return Builder.CreateFPToSI(Operand, ToTy);
      utility::unreachable();
    }
    case CastMode::ConversionUnsigned: {
      if (FromMIRTy.isIntegral() && ToMIRTy.isIntegral())
        return Builder.CreateZExt(Operand, ToTy);
      if (FromMIRTy.isIntegral() && ToMIRTy.isFloatingPoint())
        return Builder.CreateUIToFP(Operand, ToTy);
      if (FromMIRTy.isFloatingPoint() && ToMIRTy.isIntegral())
        return Builder.CreateFPToUI(Operand, ToTy);
      utility::unreachable();
    }
    case CastMode::Reinterpret: return Builder.CreateBitCast(Operand, ToTy);
    case CastMode::SatConversionSigned:   // TODO: not yet implement
    case CastMode::SatConversionUnsigned: // TODO: not yet implement
    default: utility::unreachable();
    }
  }

  llvm::Value *operator()(minsts::Extend const *Inst) {
    auto *Operand = Context[*Inst->getOperand()];
    auto OperandMIRTy = Context.getInferredType()[*Inst];
    auto *OperandTy =
        Context.getLayout().convertType(OperandMIRTy.asPrimitive());
    auto *FromTy = llvm::IntegerType::getIntNTy(
        Context.getTarget().getContext(), Inst->getFromWidth());
    llvm::Value *Result = Builder.CreateTrunc(Operand, FromTy);
    return Builder.CreateSExt(Result, OperandTy);
  }

  llvm::Value *operator()(minsts::Pack const *Inst) {
    // clang-format off
    auto Members = Inst->getArguments()
      | ranges::views::transform([&](mir::Instruction const *Argument) {
          return Context[*Argument];
        })
      | ranges::to<std::vector<llvm::Value *>>();
    auto MemberTypes = Context.getInferredType()[*Inst].asAggregate()
      | ranges::views::transform([&](bytecode::ValueType const &ValueType) {
          return Context.getLayout().convertType(ValueType);
        })
      | ranges::to<std::vector<llvm::Type *>>();
    // clang-format on
    auto *StructTy =
        llvm::StructType::get(Context.getTarget().getContext(), MemberTypes);
    llvm::Value *Result = llvm::UndefValue::get(StructTy);
    for (auto const &[Index, Member] : ranges::views::enumerate(Members)) {
      assert(Index <= std::numeric_limits<unsigned>::max());
      auto StructIndex = static_cast<unsigned>(Index);
      Result = Builder.CreateInsertValue(Result, Member, {StructIndex});
    }
    return Result;
  }

  llvm::Value *operator()(minsts::Unpack const *Inst) {
    auto *Struct = Context[*Inst->getOperand()];
    auto Index = Inst->getIndex();
    return Builder.CreateExtractValue(Struct, Index);
  }

  llvm::Value *operator()(minsts::Phi const *Inst) {
    auto *PhiTy = Context.getLayout().convertType(Inst->getType());
    auto NumCandidate = Inst->getNumCandidates();
    return Builder.CreatePHI(PhiTy, NumCandidate);
  }

  llvm::Value *visit(mir::Instruction const *Inst) {
    auto *Result = InstVisitorBase::visit(Inst);
    if (Context.getInferredType()[*Inst].isPrimitiveI32() &&
        (Result->getType() == Builder.getInt1Ty())) {
      Result = Builder.CreateZExt(Result, Context.getLayout().getI32Ty());
    }
    return Result;
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

  for (auto const *BBPtr : Context->getDominatorTree()->asPreorder())
    for (auto const &Instruction : *BBPtr) {
      if (!mir::is_a<mir::instructions::Phi>(Instruction)) continue;
      auto &PhiNode = mir::dyn_cast<mir::instructions::Phi>(Instruction);
      auto *LLVMPhi =
          llvm::dyn_cast<llvm::PHINode>(Context->operator[](Instruction));
      for (auto [Value, Path] : PhiNode.getCandidates()) {
        auto *LLVMValue = Context->operator[](*Value);
        auto [LLVMFirstBB, LLVMLastBB] = Context->operator[](*Path);
        utility::ignore(LLVMFirstBB);
        LLVMPhi->addIncoming(LLVMValue, LLVMLastBB);
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
