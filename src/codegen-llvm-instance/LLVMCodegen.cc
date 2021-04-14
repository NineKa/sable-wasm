#include "LLVMCodegen.h"

#include "../mir/Binary.h"
#include "../mir/Branch.h"
#include "../mir/Compare.h"
#include "../mir/Unary.h"
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
    IRBuilder Builder(Target);
    if (Local.isParameter()) return Target.arg_begin() + (Index + 1);
    using VKind = bytecode::ValueTypeKind;
    switch (Local.getType().getKind()) {
    case VKind::I32: return Builder.getInt32(0);
    case VKind::I64: return Builder.getInt64(0);
    case VKind::F32: return Builder.getFloat(0);
    case VKind::F64: return Builder.getDouble(0);
    case VKind::V128: return Builder.getV128(bytecode::V128Value());
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

    auto *LocalsBB = llvm::BasicBlock::Create(
        /* Context */ Target.getContext(),
        /* Name    */ "locals",
        /* Parent  */ std::addressof(Target));
    llvm::IRBuilder<> Builder(LocalsBB);
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

    auto *EntryMIRBB = std::addressof(Source.getEntryBasicBlock());
    auto [EntryLLVMBBFirst, EntryLLVMBBLast] = this->operator[](*EntryMIRBB);
    utility::ignore(EntryLLVMBBLast);
    Builder.CreateBr(EntryLLVMBBFirst);
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
    public mir::InstVisitorBase<TranslationVisitor, llvm::Value *>,
    public minsts::BranchVisitorBase<TranslationVisitor, llvm::Value *>,
    public minsts::CompareVisitorBase<TranslationVisitor, llvm::Value *>,
    public minsts::UnaryVisitorBase<TranslationVisitor, llvm::Value *>,
    public minsts::BinaryVisitorBase<TranslationVisitor, llvm::Value *> {
  FunctionTranslationTask::TranslationContext &Context;
  IRBuilder &Builder;

public:
  explicit TranslationVisitor(TranslationContext &Context_, IRBuilder &Builder_)
      : Context(Context_), Builder(Builder_) {}

  llvm::Value *operator()(minsts::Unreachable const *) {
    auto *BuiltinUnreachable =
        Context.getLayout().getBuiltin("__sable_unreachable");
    Builder.CreateCall(BuiltinUnreachable);
    return Builder.CreateUnreachable();
  }

  ///////////////////////////////// Branch /////////////////////////////////////
  llvm::Value *operator()(minsts::branch::Unconditional const *Inst) {
    auto *TargetBB = std::get<0>(Context[*Inst->getTarget()]);
    return Builder.CreateBr(TargetBB);
  }

  llvm::Value *operator()(minsts::branch::Conditional const *Inst) {
    llvm::Value *Condition = Context[*Inst->getOperand()];
    Condition = Builder.CreateICmpNE(Condition, Builder.getInt32(0));
    auto *TrueBB = std::get<0>(Context[*Inst->getTrue()]);
    auto *FalseBB = std::get<0>(Context[*Inst->getFalse()]);
    return Builder.CreateCondBr(Condition, TrueBB, FalseBB);
  }

  llvm::Value *operator()(minsts::branch::Switch const *Inst) {
    auto *Operand = Context[*Inst->getOperand()];
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
      LLVMSwitch->addCase(Builder.getInt32(Index), Target);
    return LLVMSwitch;
  }

  llvm::Value *operator()(minsts::Branch const *Inst) {
    return BranchVisitorBase::visit(Inst);
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
    if (!Context.getLayout().getTranslationOptions().SkipTblBoundaryCheck) {
      auto *BuiltinTableGuard =
          Context.getLayout().getBuiltin("__sable_table_guard");
      Builder.CreateCall(BuiltinTableGuard, {Table, Index});
    }
    auto *ExpectSignature = Builder.getCStr(
        Context.getLayout().getSignature(Inst->getExpectType()),
        "indirect.call.signature");
    auto *BuiltinTableCheck =
        Context.getLayout().getBuiltin("__sable_table_check");
    Builder.CreateCall(BuiltinTableCheck, {Table, Index, ExpectSignature});
    auto *BuiltinTableFunction =
        Context.getLayout().getBuiltin("__sable_table_function");
    auto *BuiltinTableContext =
        Context.getLayout().getBuiltin("__sable_table_context");

    llvm::Value *CalleeContext =
        Builder.CreateCall(BuiltinTableContext, {Table, Index});
    auto *CalleeFunction =
        Builder.CreateCall(BuiltinTableFunction, {Table, Index});

    auto *IsNullTest = Builder.CreateIsNull(CalleeContext);
    CalleeContext =
        Builder.CreateSelect(IsNullTest, InstancePtr, CalleeContext);

    std::vector<llvm::Value *> Arguments;
    Arguments.reserve(Inst->getNumArguments() + 1);
    Arguments.push_back(CalleeContext);
    for (auto const *Argument : Inst->getArguments())
      Arguments.push_back(Context[*Argument]);

    auto *CalleeTy = Context.getLayout().convertType(Inst->getExpectType());
    auto *CalleePtrTy = llvm::PointerType::getUnqual(CalleeTy);
    auto *CalleePtr = Builder.CreatePointerCast(CalleeFunction, CalleePtrTy);
    llvm::FunctionCallee Callee(CalleeTy, CalleePtr);
    return Builder.CreateCall(Callee, Arguments);
  }

  llvm::Value *operator()(minsts::Select const *Inst) {
    llvm::Value *Condition = Context[*Inst->getCondition()];
    Condition = Builder.CreateICmpNE(Condition, Builder.getInt32(0));
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
    case VKind::I32: return Builder.getInt32(Inst->asI32());
    case VKind::I64: return Builder.getInt64(Inst->asI64());
    case VKind::F32: return Builder.getFloat(Inst->asF32());
    case VKind::F64: return Builder.getDouble(Inst->asF64());
    case VKind::V128: return Builder.getV128(Inst->asV128());
    default: utility::unreachable();
    }
  }

  ///////////////////////////////// Compare ////////////////////////////////////
  llvm::Value *operator()(minsts::compare::IntCompare const *Inst) {
    auto *LHS = Context[*Inst->getLHS()];
    auto *RHS = Context[*Inst->getRHS()];
    using CompareOp = minsts::compare::IntCompareOperator;
    // clang-format off
    switch (Inst->getOperator()) {
    case CompareOp::Eq : return Builder.CreateICmpEQ (LHS, RHS);
    case CompareOp::Ne : return Builder.CreateICmpNE (LHS, RHS);
    case CompareOp::LtS: return Builder.CreateICmpSLT(LHS, RHS);
    case CompareOp::LtU: return Builder.CreateICmpULT(LHS, RHS);
    case CompareOp::GtS: return Builder.CreateICmpSGT(LHS, RHS);
    case CompareOp::GtU: return Builder.CreateICmpUGT(LHS, RHS);
    case CompareOp::LeS: return Builder.CreateICmpSLE(LHS, RHS);
    case CompareOp::LeU: return Builder.CreateICmpULE(LHS, RHS);
    case CompareOp::GeS: return Builder.CreateICmpSGE(LHS, RHS);
    case CompareOp::GeU: return Builder.CreateICmpUGE(LHS, RHS);
    default: utility::unreachable();
    }
    // clang-format on
  }

  llvm::Value *operator()(minsts::compare::FPCompare const *Inst) {
    auto *LHS = Context[*Inst->getLHS()];
    auto *RHS = Context[*Inst->getRHS()];
    using CompareOp = minsts::compare::FPCompareOperator;
    switch (Inst->getOperator()) {
    case CompareOp::Eq: return Builder.CreateFCmpUEQ(LHS, RHS);
    case CompareOp::Ne: return Builder.CreateFCmpUNE(LHS, RHS);
    case CompareOp::Lt: return Builder.CreateFCmpULT(LHS, RHS);
    case CompareOp::Gt: return Builder.CreateFCmpUGT(LHS, RHS);
    case CompareOp::Le: return Builder.CreateFCmpULE(LHS, RHS);
    case CompareOp::Ge: return Builder.CreateFCmpUGE(LHS, RHS);
    default: utility::unreachable();
    }
  }

  llvm::Value *operator()(minsts::compare::SIMD128IntCompare const *Inst) {
    auto *LHS = Context[*Inst->getLHS()];
    auto *RHS = Context[*Inst->getRHS()];
    auto *ExpectVecType = Builder.getV128Ty(Inst->getLaneInfo());
    if (LHS->getType() != ExpectVecType)
      LHS = Builder.CreateBitCast(LHS, ExpectVecType);
    if (RHS->getType() != ExpectVecType)
      LHS = Builder.CreateBitCast(RHS, ExpectVecType);
    llvm::Value *Result = nullptr;
    using Operator = mir::instructions::compare::SIMD128IntCompareOperator;
    // clang-format off
    switch (Inst->getOperator()) {
    case Operator::Eq : Result = Builder.CreateICmpEQ (LHS, RHS); break;
    case Operator::Ne : Result = Builder.CreateICmpNE (LHS, RHS); break;
    case Operator::LtS: Result = Builder.CreateICmpSLT(LHS, RHS); break;
    case Operator::LtU: Result = Builder.CreateICmpULT(LHS, RHS); break;
    case Operator::GtS: Result = Builder.CreateICmpSGT(LHS, RHS); break;
    case Operator::GtU: Result = Builder.CreateICmpUGT(LHS, RHS); break;
    case Operator::LeS: Result = Builder.CreateICmpSLE(LHS, RHS); break;
    case Operator::LeU: Result = Builder.CreateICmpULE(LHS, RHS); break;
    case Operator::GeS: Result = Builder.CreateICmpSGE(LHS, RHS); break;
    case Operator::GeU: Result = Builder.CreateICmpUGE(LHS, RHS); break;
    default: utility::unreachable();
    }
    // clang-format on
    Result = Builder.CreateSExt(Result, ExpectVecType);
    return Result;
  }

  llvm::Value *operator()(minsts::compare::SIMD128FPCompare const *Inst) {
    auto *LHS = Context[*Inst->getLHS()];
    auto *RHS = Context[*Inst->getRHS()];
    auto *ExpectVecType = Builder.getV128Ty(Inst->getLaneInfo());
    if (LHS->getType() != ExpectVecType)
      LHS = Builder.CreateBitCast(LHS, ExpectVecType);
    if (RHS->getType() != ExpectVecType)
      RHS = Builder.CreateBitCast(RHS, ExpectVecType);
    llvm::Value *Result = nullptr;
    using Operator = mir::instructions::compare::SIMD128FPCompareOperator;
    switch (Inst->getOperator()) {
    case Operator::Eq: Result = Builder.CreateFCmpUEQ(LHS, RHS); break;
    case Operator::Ne: Result = Builder.CreateFCmpUNE(LHS, RHS); break;
    case Operator::Lt: Result = Builder.CreateFCmpULT(LHS, RHS); break;
    case Operator::Gt: Result = Builder.CreateFCmpUGT(LHS, RHS); break;
    case Operator::Le: Result = Builder.CreateFCmpULE(LHS, RHS); break;
    case Operator::Ge: Result = Builder.CreateFCmpUGE(LHS, RHS); break;
    default: utility::unreachable();
    }
    auto ResultLaneInfo = Inst->getLaneInfo().getCmpResultLaneInfo();
    auto *ExpectResultTy = Builder.getV128Ty(ResultLaneInfo);
    Result = Builder.CreateSExt(Result, ExpectResultTy);
    return Result;
  }

  llvm::Value *operator()(minsts::Compare const *Inst) {
    return CompareVisitorBase::visit(Inst);
  }

  template <llvm::Intrinsic::ID IntrinsicID, typename... ArgTypes>
  llvm::Function *getIntrinsic(ArgTypes &&...Args) {
    auto *EnclosingModule = Context.getTarget().getParent();
    std::array<llvm::Type *, sizeof...(ArgTypes)> IntrinsicArgumentTys{
        std::forward<ArgTypes>(Args)...};
    return llvm::Intrinsic::getDeclaration(
        EnclosingModule, IntrinsicID, IntrinsicArgumentTys);
  }

  ////////////////////////////////// Unary /////////////////////////////////////
  llvm::Value *operator()(minsts::unary::IntUnary const *Inst) {
    auto *MIROperand = Inst->getOperand();
    auto *Operand = Context[*MIROperand];
    using UnaryOperator = minsts::unary::IntUnaryOperator;
    switch (Inst->getOperator()) {
    case UnaryOperator::Eqz: {
      auto OperandType = Context.getInferredType()[*MIROperand];
      llvm::Value *Zero = nullptr;
      using VKind = bytecode::ValueTypeKind;
      switch (OperandType.asPrimitive().getKind()) {
      case VKind::I32: Zero = Builder.getInt32(0); break;
      case VKind::I64: Zero = Builder.getInt64(0); break;
      default: utility::unreachable();
      }
      return Builder.CreateICmpEQ(Operand, Zero);
    }
    case UnaryOperator::Clz: return Builder.CreateIntrinsicClz(Operand);
    case UnaryOperator::Ctz: return Builder.CreateIntrinsicCtz(Operand);
    case UnaryOperator::Popcnt: return Builder.CreateIntrinsicPopcnt(Operand);
    default: utility::unreachable();
    }
  }

  llvm::Value *operator()(minsts::unary::FPUnary const *Inst) {
    auto *Operand = Context[*Inst->getOperand()];
    using UnaryOperator = mir::instructions::unary::FPUnaryOperator;
    // clang-format off
    switch (Inst->getOperator()) {
    case UnaryOperator::Abs    : return Builder.CreateIntrinsicFPAbs(Operand);
    case UnaryOperator::Neg    : return Builder.CreateFNeg(Operand);
    case UnaryOperator::Ceil   : return Builder.CreateIntrinsicCeil(Operand);
    case UnaryOperator::Floor  : return Builder.CreateIntrinsicFloor(Operand);
    case UnaryOperator::Trunc  : return Builder.CreateIntrinsicTrunc(Operand);
    case UnaryOperator::Nearest: return Builder.CreateIntrinsicNearest(Operand);
    case UnaryOperator::Sqrt   : return Builder.CreateIntrinsicSqrt(Operand);
    default: utility::unreachable();
    }
    // clang-format on
  }

  llvm::Value *operator()(minsts::unary::SIMD128Unary const *Inst) {
    auto *Operand = Context[*Inst->getOperand()];
    using UnaryOperator = mir::instructions::unary::SIMD128UnaryOperator;
    if (Operand->getType() != Builder.getInt128Ty())
      Operand = Builder.CreateBitCast(Operand, Builder.getInt128Ty());
    switch (Inst->getOperator()) {
    case UnaryOperator::AnyTrue: {
      auto *Zero = Builder.getIntN(128, 0);
      llvm::Value *Result = Builder.CreateICmpEQ(Operand, Zero);
      return Builder.CreateNot(Result);
    }
    case UnaryOperator::Not: return Builder.CreateNot(Operand);
    default: utility::unreachable();
    }
  }

  llvm::Value *operator()(minsts::unary::SIMD128IntUnary const *Inst) {
    auto *Operand = Context[*Inst->getOperand()];
    auto *ExpectTy = Builder.getV128Ty(Inst->getLaneInfo());
    if (Operand->getType() != ExpectTy)
      Operand = Builder.CreateBitCast(Operand, ExpectTy);
    using UnaryOperator = mir::instructions::unary::SIMD128IntUnaryOperator;
    switch (Inst->getOperator()) {
    case UnaryOperator::Neg: {
      auto *Zero = Builder.getV128(bytecode::V128Value(), Inst->getLaneInfo());
      return Builder.CreateSub(Zero, Operand);
    }
    case UnaryOperator::Abs: {
      auto *Zero = Builder.getV128(bytecode::V128Value(), Inst->getLaneInfo());
      auto *Neg = Builder.CreateSub(Zero, Operand);
      auto *SelectVector = Builder.CreateICmpSGT(Operand, Zero);
      return Builder.CreateSelect(SelectVector, Operand, Neg);
    }
    case UnaryOperator::AllTrue: {
      auto *Zero = Builder.getV128(bytecode::V128Value(), Inst->getLaneInfo());
      auto *CmpVector = Builder.CreateICmpNE(Operand, Zero);
      return Builder.CreateIntrinsicReduceAnd(CmpVector);
    }
    case UnaryOperator::Bitmask: {
      auto LaneWidth = Inst->getLaneInfo().getLaneWidth();
      auto NumLane = Inst->getLaneInfo().getNumLane();
      auto *ShiftAmount = Builder.getInt32(LaneWidth - 1);
      llvm::Value *Result = Builder.CreateLShr(Operand, ShiftAmount);
      auto *TruncToTy =
          llvm::FixedVectorType::get(Builder.getInt1Ty(), NumLane);
      Result = Builder.CreateTrunc(Result, TruncToTy);
      Result = Builder.CreateBitCast(Result, Builder.getIntNTy(NumLane));
      return Builder.CreateZExt(Result, Builder.getInt32Ty());
    }
    default: utility::unreachable();
    }
  }

  llvm::Value *operator()(minsts::unary::SIMD128FPUnary const *Inst) {
    auto *Operand = Context[*Inst->getOperand()];
    auto *ExpectTy = Builder.getV128Ty(Inst->getLaneInfo());
    if (Operand->getType() != ExpectTy)
      Operand = Builder.CreateBitCast(Operand, ExpectTy);
    using UnaryOperator = mir::instructions::unary::SIMD128FPUnaryOperator;
    // clang-format off
    switch (Inst->getOperator()) {
    case UnaryOperator::Neg    : return Builder.CreateFNeg(Operand);
    case UnaryOperator::Abs    : return Builder.CreateIntrinsicFPAbs(Operand);
    case UnaryOperator::Sqrt   : return Builder.CreateIntrinsicSqrt(Operand);
    case UnaryOperator::Ceil   : return Builder.CreateIntrinsicCeil(Operand);
    case UnaryOperator::Floor  : return Builder.CreateIntrinsicFloor(Operand);
    case UnaryOperator::Trunc  : return Builder.CreateIntrinsicTrunc(Operand);
    case UnaryOperator::Nearest: return Builder.CreateIntrinsicNearest(Operand);
    default: utility::unreachable();
    }
    // clang-format on
  }

  llvm::Value *operator()(minsts::Unary const *Inst) {
    return UnaryVisitorBase::visit(Inst);
  }

  ///////////////////////////////// Binary /////////////////////////////////////
  llvm::Value *operator()(minsts::binary::IntBinary const *Inst) {
    auto *MIRValueLHS = Inst->getLHS();
    auto *MIRValueRHS = Inst->getRHS();
    auto *LHS = Context[*MIRValueLHS];
    auto *RHS = Context[*MIRValueRHS];
    using IntBinaryOperator = mir::instructions::binary::IntBinaryOperator;
    switch (Inst->getOperator()) {
    // clang-format off
    case IntBinaryOperator::Add : return Builder.CreateAdd (LHS, RHS);
    case IntBinaryOperator::Sub : return Builder.CreateSub (LHS, RHS);
    case IntBinaryOperator::Mul : return Builder.CreateMul (LHS, RHS);
    case IntBinaryOperator::DivS: return Builder.CreateSDiv(LHS, RHS);
    case IntBinaryOperator::DivU: return Builder.CreateUDiv(LHS, RHS);
    case IntBinaryOperator::RemS: return Builder.CreateSRem(LHS, RHS);
    case IntBinaryOperator::RemU: return Builder.CreateURem(LHS, RHS);
    case IntBinaryOperator::And : return Builder.CreateAnd (LHS, RHS);
    case IntBinaryOperator::Or  : return Builder.CreateOr  (LHS, RHS);
    case IntBinaryOperator::Xor : return Builder.CreateXor (LHS, RHS);
    case IntBinaryOperator::Shl : return Builder.CreateShl (LHS, RHS);
    case IntBinaryOperator::ShrS: return Builder.CreateAShr(LHS, RHS);
    case IntBinaryOperator::ShrU: return Builder.CreateLShr(LHS, RHS);
    // clang-format on
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

  llvm::Value *operator()(minsts::binary::FPBinary const *Inst) {
    auto *MIRValueLHS = Inst->getLHS();
    auto *MIRValueRHS = Inst->getRHS();
    auto *LHS = Context[*MIRValueLHS];
    auto *RHS = Context[*MIRValueRHS];
    using FPBinaryOperator = mir::instructions::binary::FPBinaryOperator;
    switch (Inst->getOperator()) {
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

  llvm::Value *operator()(minsts::Binary const *Inst) {
    return BinaryVisitorBase::visit(Inst);
  }

  llvm::Value *
  getMemoryLocation(mir::Memory const &Memory, llvm::Value *Offset) {
    auto *InstancePtr = Context.getInstancePtr();
    auto *PtrIntTy = Builder.getIntPtrTy();
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
      if (!Context.getLayout().getTranslationOptions().AssumeMemRWAligned)
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
      if (!Context.getLayout().getTranslationOptions().AssumeMemRWAligned)
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
    if (!Context.getLayout().getTranslationOptions().AssumeMemRWAligned)
      Result->setAlignment(llvm::Align(1));
    return Result;
  }

  llvm::Value *operator()(minsts::MemoryGuard const *Inst) {
    if (Context.getLayout().getTranslationOptions().SkipMemBoundaryCheck)
      return nullptr;
    auto *InstancePtr = Context.getInstancePtr();
    auto *BuiltinMemoryGuard =
        Context.getLayout().getBuiltin("__sable_memory_guard");
    auto *Memory =
        Context.getLayout().get(Builder, InstancePtr, *Inst->getLinearMemory());
    llvm::Value *Offset = Context[*Inst->getAddress()];
    auto *GuardSize = Builder.getInt32(Inst->getGuardSize());
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
      Result = Builder.CreateZExt(Result, Builder.getInt32Ty());
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
    IRBuilder Builder(*FirstBBPtr);
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
    mir::Module const &Source_, llvm::Module &Target_,
    TranslationOptions Options_)
    : Layout(nullptr), Source(std::addressof(Source_)),
      Target(std::addressof(Target_)), Options(Options_) {}

void ModuleTranslationTask::perform() {
  Layout = std::make_unique<EntityLayout>(*Source, *Target, Options);
  for (auto const &Function : Source->getFunctions().asView()) {
    if (Function.isDeclaration()) continue;
    auto &TargetFunction = *Layout->operator[](Function).definition();
    FunctionTranslationTask Task(*Layout, Function, TargetFunction);
    Task.perform();
  }
}

} // namespace codegen::llvm_instance
