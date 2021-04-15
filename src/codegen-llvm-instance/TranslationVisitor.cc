#include "TranslationVisitor.h"

#include "LLVMCodegen.h"
#include "TranslationContext.h"

#include <llvm/IR/DerivedTypes.h>

#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/transform.hpp>

namespace codegen::llvm_instance {
namespace minsts = mir::instructions;

TranslationVisitor::TranslationVisitor(
    TranslationContext &Context_, IRBuilder &Builder_)
    : Context(Context_), Builder(Builder_) {}

TranslationVisitor::~TranslationVisitor() noexcept = default;

llvm::Value *TranslationVisitor::operator()(minsts::Unreachable const *) {
  auto *BuiltinUnreachable =
      Context.getLayout().getBuiltin("__sable_unreachable");
  Builder.CreateCall(BuiltinUnreachable);
  return Builder.CreateUnreachable();
}

////////////////////////////////// Branch //////////////////////////////////////
llvm::Value *
TranslationVisitor::operator()(minsts::branch::Unconditional const *Inst) {
  auto [TargetBBFirst, TargetBBLast] = Context[*Inst->getTarget()];
  utility::ignore(TargetBBLast);
  return Builder.CreateBr(TargetBBFirst);
}

llvm::Value *
TranslationVisitor::operator()(minsts::branch::Conditional const *Inst) {
  auto *Condition = Context[*Inst->getOperand()];
  Condition = Builder.CreateICmpNE(Condition, Builder.getInt32(0));
  auto *TrueBB = std::get<0>(Context[*Inst->getTrue()]);
  auto *FalseBB = std::get<0>(Context[*Inst->getFalse()]);
  return Builder.CreateCondBr(Condition, TrueBB, FalseBB);
}

llvm::Value *
TranslationVisitor::operator()(minsts::branch::Switch const *Inst) {
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

llvm::Value *TranslationVisitor::operator()(minsts::Branch const *Inst) {
  return BranchVisitorBase::visit(Inst);
}

llvm::Value *TranslationVisitor::operator()(minsts::Return const *Inst) {
  if (Inst->hasReturnValue()) {
    auto *ReturnValue = Context[*Inst->getOperand()];
    auto ReturnTy = Context.getInferredType()[*Inst->getOperand()];
    return Builder.CreateRet(ReturnValue);
  }
  return Builder.CreateRetVoid();
}

llvm::Value *TranslationVisitor::operator()(minsts::Call const *Inst) {
  auto *InstancePtr = Context.getInstancePtr();
  auto *Callee = Context.getLayout()[*Inst->getTarget()].definition();
  std::vector<llvm::Value *> Arguments;
  Arguments.reserve(Inst->getTarget()->getType().getNumParameter() + 1);
  Arguments.push_back(InstancePtr);
  for (auto const *Argument : Inst->getArguments())
    Arguments.push_back(Context[*Argument]);
  return Builder.CreateCall(Callee, Arguments);
}

llvm::Value *TranslationVisitor::operator()(minsts::CallIndirect const *Inst) {
  auto *InstancePtr = Context.getInstancePtr();
  auto *Index = Context[*Inst->getOperand()];
  auto *Table =
      Context.getLayout().get(Builder, InstancePtr, *Inst->getIndirectTable());
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
  CalleeContext = Builder.CreateSelect(IsNullTest, InstancePtr, CalleeContext);

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

llvm::Value *TranslationVisitor::operator()(minsts::Select const *Inst) {
  auto *Condition = Context[*Inst->getCondition()];
  Condition = Builder.CreateICmpNE(Condition, Builder.getInt32(0));
  auto *True = Context[*Inst->getTrue()];
  auto *False = Context[*Inst->getFalse()];
  return Builder.CreateSelect(Condition, True, False);
}

llvm::Value *TranslationVisitor::operator()(minsts::LocalGet const *Inst) {
  auto *Local = Context[*Inst->getTarget()];
  return Builder.CreateLoad(Local);
}

llvm::Value *TranslationVisitor::operator()(minsts::LocalSet const *Inst) {
  auto *Local = Context[*Inst->getTarget()];
  auto *Value = Context[*Inst->getOperand()];
  return Builder.CreateStore(Value, Local);
}

llvm::Value *TranslationVisitor::operator()(minsts::GlobalGet const *Inst) {
  auto *InstancePtr = Context.getInstancePtr();
  auto *Global =
      Context.getLayout().get(Builder, InstancePtr, *Inst->getTarget());
  auto GlobalValueType = Inst->getTarget()->getType().getType();
  auto *GlobalType = Context.getLayout().convertType(GlobalValueType);
  auto *GlobalPtrType = llvm::PointerType::getUnqual(GlobalType);
  Global = Builder.CreatePointerCast(Global, GlobalPtrType);
  return Builder.CreateLoad(Global);
}

llvm::Value *TranslationVisitor::operator()(minsts::GlobalSet const *Inst) {
  auto *InstancePtr = Context.getInstancePtr();
  auto *Value = Context[*Inst->getOperand()];
  auto *Global =
      Context.getLayout().get(Builder, InstancePtr, *Inst->getTarget());
  auto GlobalValueType = Inst->getTarget()->getType().getType();
  auto *GlobalType = Context.getLayout().convertType(GlobalValueType);
  auto *GlobalPtrType = llvm::PointerType::getUnqual(GlobalType);
  Global = Builder.CreatePointerCast(Global, GlobalPtrType);
  return Builder.CreateStore(Value, Global);
}

llvm::Value *TranslationVisitor::operator()(minsts::Constant const *Inst) {
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

////////////////////////////////// Compare /////////////////////////////////////
llvm::Value *
TranslationVisitor::operator()(minsts::compare::IntCompare const *Inst) {
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

llvm::Value *
TranslationVisitor::operator()(minsts::compare::FPCompare const *Inst) {
  auto *LHS = Context[*Inst->getLHS()];
  auto *RHS = Context[*Inst->getRHS()];
  using CompareOp = minsts::compare::FPCompareOperator;
  switch (Inst->getOperator()) {
  case CompareOp::Eq: return Builder.CreateFCmpOEQ(LHS, RHS);
  case CompareOp::Ne: return Builder.CreateFCmpONE(LHS, RHS);
  case CompareOp::Lt: return Builder.CreateFCmpOLT(LHS, RHS);
  case CompareOp::Gt: return Builder.CreateFCmpOGT(LHS, RHS);
  case CompareOp::Le: return Builder.CreateFCmpOLE(LHS, RHS);
  case CompareOp::Ge: return Builder.CreateFCmpOGE(LHS, RHS);
  default: utility::unreachable();
  }
}

llvm::Value *
TranslationVisitor::operator()(minsts::compare::SIMD128IntCompare const *Inst) {
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

llvm::Value *
TranslationVisitor::operator()(minsts::compare::SIMD128FPCompare const *Inst) {
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
  case Operator::Eq: Result = Builder.CreateFCmpOEQ(LHS, RHS); break;
  case Operator::Ne: Result = Builder.CreateFCmpONE(LHS, RHS); break;
  case Operator::Lt: Result = Builder.CreateFCmpOLT(LHS, RHS); break;
  case Operator::Gt: Result = Builder.CreateFCmpOGT(LHS, RHS); break;
  case Operator::Le: Result = Builder.CreateFCmpOLE(LHS, RHS); break;
  case Operator::Ge: Result = Builder.CreateFCmpOGE(LHS, RHS); break;
  default: utility::unreachable();
  }
  auto ResultLaneInfo = Inst->getLaneInfo().getCmpResultLaneInfo();
  auto *ExpectResultTy = Builder.getV128Ty(ResultLaneInfo);
  Result = Builder.CreateSExt(Result, ExpectResultTy);
  return Result;
}

llvm::Value *TranslationVisitor::operator()(minsts::Compare const *Inst) {
  return CompareVisitorBase::visit(Inst);
}

/////////////////////////////////// Unary //////////////////////////////////////
llvm::Value *
TranslationVisitor::operator()(minsts::unary::IntUnary const *Inst) {
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

llvm::Value *
TranslationVisitor::operator()(minsts::unary::FPUnary const *Inst) {
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

llvm::Value *
TranslationVisitor::operator()(minsts::unary::SIMD128Unary const *Inst) {
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

llvm::Value *
TranslationVisitor::operator()(minsts::unary::SIMD128IntUnary const *Inst) {
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
  case UnaryOperator::Abs: return Builder.CreateIntrinsicIntAbs(Operand);
  case UnaryOperator::AllTrue: {
    auto *Zero = Builder.getV128(bytecode::V128Value(), Inst->getLaneInfo());
    auto *CmpVector = Builder.CreateICmpNE(Operand, Zero);
    return Builder.CreateAddReduce(CmpVector);
  }
  case UnaryOperator::Bitmask: {
    auto LaneWidth = Inst->getLaneInfo().getLaneWidth();
    auto NumLane = Inst->getLaneInfo().getNumLane();
    auto *ShiftAmount = Builder.getInt32(LaneWidth - 1);
    llvm::Value *Result = Builder.CreateLShr(Operand, ShiftAmount);
    auto *TruncToTy = llvm::FixedVectorType::get(Builder.getInt1Ty(), NumLane);
    Result = Builder.CreateTrunc(Result, TruncToTy);
    Result = Builder.CreateBitCast(Result, Builder.getIntNTy(NumLane));
    return Builder.CreateZExt(Result, Builder.getInt32Ty());
  }
  default: utility::unreachable();
  }
}

llvm::Value *
TranslationVisitor::operator()(minsts::unary::SIMD128FPUnary const *Inst) {
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

llvm::Value *TranslationVisitor::operator()(minsts::Unary const *Inst) {
  return UnaryVisitorBase::visit(Inst);
}

////////////////////////////////// Binary //////////////////////////////////////
llvm::Value *
TranslationVisitor::operator()(minsts::binary::IntBinary const *Inst) {
  auto *LHS = Context[*Inst->getLHS()];
  auto *RHS = Context[*Inst->getRHS()];
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
  case IntBinaryOperator::Rotl:
    return Builder.CreateIntrinsicFShl(LHS, LHS, RHS);
  case IntBinaryOperator::Rotr:
    return Builder.CreateIntrinsicFShr(LHS, LHS, RHS);
  default: utility::unreachable();
  }
}

llvm::Value *
TranslationVisitor::operator()(minsts::binary::FPBinary const *Inst) {
  auto *LHS = Context[*Inst->getLHS()];
  auto *RHS = Context[*Inst->getRHS()];
  using FPBinaryOperator = mir::instructions::binary::FPBinaryOperator;
  switch (Inst->getOperator()) {
  case FPBinaryOperator::Add: return Builder.CreateFAdd(LHS, RHS);
  case FPBinaryOperator::Sub: return Builder.CreateFSub(LHS, RHS);
  case FPBinaryOperator::Mul: return Builder.CreateFMul(LHS, RHS);
  case FPBinaryOperator::Div: return Builder.CreateFDiv(LHS, RHS);
  case FPBinaryOperator::Min: return Builder.CreateMinimum(LHS, RHS);
  case FPBinaryOperator::Max: return Builder.CreateMaximum(LHS, RHS);
  case FPBinaryOperator::CopySign:
    return Builder.CreateIntrinsicCopysign(LHS, RHS);
  default: utility::unreachable();
  }
}

llvm::Value *
TranslationVisitor::operator()(minsts::binary::SIMD128Binary const *Inst) {
  auto *LHS = Context[*Inst->getLHS()];
  auto *RHS = Context[*Inst->getRHS()];
  if (LHS->getType() != Builder.getInt128Ty())
    LHS = Builder.CreateBitCast(LHS, Builder.getInt128Ty());
  if (RHS->getType() != Builder.getInt128Ty())
    RHS = Builder.CreateBitCast(RHS, Builder.getInt128Ty());
  using BinaryOperator = mir::instructions::binary::SIMD128BinaryOperator;
  switch (Inst->getOperator()) {
  case BinaryOperator::And: return Builder.CreateAnd(LHS, RHS);
  case BinaryOperator::Or: return Builder.CreateOr(LHS, RHS);
  case BinaryOperator::Xor: return Builder.CreateXor(LHS, RHS);
  case BinaryOperator::AndNot: {
    RHS = Builder.CreateNot(RHS);
    return Builder.CreateAnd(LHS, RHS);
  }
  default: utility::unreachable();
  }
}

namespace {
enum class SliceMode { Low, High, Even, Odd };

template <SliceMode SliceMode_, bool Signed_>
llvm::Value *sliceThenExtend(
    IRBuilder &Builder, llvm::Value *Vector,
    mir::SIMD128IntLaneInfo const &LaneInfo) {
  auto *WidenTy = Builder.getV128Ty(LaneInfo.widen());
  switch (SliceMode_) {
  case SliceMode::Low: Vector = Builder.CreateVectorSliceLow(Vector); break;
  case SliceMode::High: Vector = Builder.CreateVectorSliceHigh(Vector); break;
  case SliceMode::Odd: Vector = Builder.CreateVectorSliceOdd(Vector); break;
  case SliceMode::Even: Vector = Builder.CreateVectorSliceEven(Vector); break;
  default: utility::unreachable();
  }
  if constexpr (Signed_) {
    Vector = Builder.CreateSExt(Vector, WidenTy);
  } else {
    Vector = Builder.CreateZExt(Vector, WidenTy);
  }
  return Vector;
}
} // namespace

llvm::Value *
TranslationVisitor::operator()(minsts::binary::SIMD128IntBinary const *Inst) {
  auto *LHS = Context[*Inst->getLHS()];
  auto *RHS = Context[*Inst->getRHS()];
  auto LaneInfo = Inst->getLaneInfo();
  auto *ExpectOperandTy = Builder.getV128Ty(LaneInfo);
  if (LHS->getType() != ExpectOperandTy)
    LHS = Builder.CreateBitCast(LHS, ExpectOperandTy);
  if (RHS->getType() != ExpectOperandTy &&
      Context.getInferredType()[*Inst->getRHS()].isPrimitiveV128())
    RHS = Builder.CreateBitCast(RHS, ExpectOperandTy);
  using BinaryOperator = mir::instructions::binary::SIMD128IntBinaryOperator;
  switch (Inst->getOperator()) {
  case BinaryOperator::Add: return Builder.CreateAdd(LHS, RHS);
  case BinaryOperator::Sub: return Builder.CreateSub(LHS, RHS);
  case BinaryOperator::Mul: return Builder.CreateMul(LHS, RHS);
  case BinaryOperator::ExtMulLowS: {
    LHS = sliceThenExtend<SliceMode::Low, true>(Builder, LHS, LaneInfo);
    RHS = sliceThenExtend<SliceMode::Low, true>(Builder, RHS, LaneInfo);
    return Builder.CreateMul(LHS, RHS);
  }
  case BinaryOperator::ExtMulLowU: {
    LHS = sliceThenExtend<SliceMode::Low, false>(Builder, LHS, LaneInfo);
    RHS = sliceThenExtend<SliceMode::Low, false>(Builder, RHS, LaneInfo);
    return Builder.CreateMul(LHS, RHS);
  }
  case BinaryOperator::ExtMulHighS: {
    LHS = sliceThenExtend<SliceMode::High, true>(Builder, LHS, LaneInfo);
    RHS = sliceThenExtend<SliceMode::High, true>(Builder, RHS, LaneInfo);
    return Builder.CreateMul(LHS, RHS);
  }
  case BinaryOperator::ExtMulHighU: {
    LHS = sliceThenExtend<SliceMode::High, false>(Builder, LHS, LaneInfo);
    RHS = sliceThenExtend<SliceMode::High, false>(Builder, RHS, LaneInfo);
    return Builder.CreateMul(LHS, RHS);
  }
  case BinaryOperator::ExtAddPairwiseS: {
    LHS = sliceThenExtend<SliceMode::Odd, true>(Builder, LHS, LaneInfo);
    RHS = sliceThenExtend<SliceMode::Even, true>(Builder, RHS, LaneInfo);
    return Builder.CreateAdd(LHS, RHS);
  }
  case BinaryOperator::ExtAddPairwiseU: {
    LHS = sliceThenExtend<SliceMode::Odd, false>(Builder, LHS, LaneInfo);
    RHS = sliceThenExtend<SliceMode::Even, false>(Builder, RHS, LaneInfo);
    return Builder.CreateAdd(LHS, RHS);
  }
  case BinaryOperator::AddSatS: return Builder.CreateIntrinsicAddSatS(LHS, RHS);
  case BinaryOperator::AddSatU: return Builder.CreateIntrinsicAddSatU(LHS, RHS);
  case BinaryOperator::SubSatS: return Builder.CreateIntrinsicSubSatS(LHS, RHS);
  case BinaryOperator::SubSatU: return Builder.CreateIntrinsicSubSatU(LHS, RHS);
  case BinaryOperator::Shl: {
    assert(RHS->getType()->isIntegerTy());
    RHS = Builder.CreateVectorSplat(ExpectOperandTy->getElementCount(), RHS);
    return Builder.CreateShl(LHS, RHS);
  }
  case BinaryOperator::ShrS: {
    assert(RHS->getType()->isIntegerTy());
    RHS = Builder.CreateVectorSplat(ExpectOperandTy->getElementCount(), RHS);
    return Builder.CreateAShr(LHS, RHS);
  }
  case BinaryOperator::ShrU: {
    assert(RHS->getType()->isIntegerTy());
    RHS = Builder.CreateVectorSplat(ExpectOperandTy->getElementCount(), RHS);
    return Builder.CreateLShr(LHS, RHS);
  }
  case BinaryOperator::MinS: return Builder.CreateIntrinsicIntMinS(LHS, RHS);
  case BinaryOperator::MinU: return Builder.CreateIntrinsicIntMinU(LHS, RHS);
  case BinaryOperator::MaxS: return Builder.CreateIntrinsicIntMaxS(LHS, RHS);
  case BinaryOperator::MaxU: return Builder.CreateIntrinsicIntMaxU(LHS, RHS);
  case BinaryOperator::AvgrU: {
    // TODO: Better Strategy? Currently implement as (LHS + RHS + 1) >> 1;
    auto *ElementTy =
        llvm::dyn_cast<llvm::IntegerType>(ExpectOperandTy->getElementType());
    auto NumLane = ExpectOperandTy->getElementCount();
    auto *One = Builder.getIntN(ElementTy->getBitWidth(), 1);
    auto *Ones = llvm::ConstantVector::getSplat(NumLane, One);
    auto *Result = Builder.CreateAdd(LHS, RHS);
    Result = Builder.CreateAdd(Result, Ones);
    Result = Builder.CreateLShr(Result, Ones);
    return Result;
  }
  default: utility::unreachable();
  }
}

llvm::Value *
TranslationVisitor::operator()(minsts::binary::SIMD128FPBinary const *Inst) {
  auto *LHS = Context[*Inst->getLHS()];
  auto *RHS = Context[*Inst->getRHS()];
  auto *ExpectOperandTy = Builder.getV128Ty(Inst->getLaneInfo());
  if (LHS->getType() != ExpectOperandTy)
    LHS = Builder.CreateBitCast(LHS, ExpectOperandTy);
  if (RHS->getType() != ExpectOperandTy)
    RHS = Builder.CreateBitCast(RHS, ExpectOperandTy);
  using BinaryOperator = mir::instructions::binary::SIMD128FPBinaryOperator;
  switch (Inst->getOperator()) {
  case BinaryOperator::Add: return Builder.CreateFAdd(LHS, RHS);
  case BinaryOperator::Sub: return Builder.CreateFSub(LHS, RHS);
  case BinaryOperator::Mul: return Builder.CreateFMul(LHS, RHS);
  case BinaryOperator::Div: return Builder.CreateFDiv(LHS, RHS);
  case BinaryOperator::Min: return Builder.CreateMinimum(LHS, RHS);
  case BinaryOperator::Max: return Builder.CreateMaximum(LHS, RHS);
  case BinaryOperator::PMin: {
    auto *CmpVector = Builder.CreateFCmpOLT(RHS, LHS);
    return Builder.CreateSelect(CmpVector, RHS, LHS);
  }
  case BinaryOperator::PMax: {
    auto *CmpVector = Builder.CreateFCmpOLT(LHS, RHS);
    return Builder.CreateSelect(CmpVector, RHS, LHS);
  }
  default: utility::unreachable();
  }
}

llvm::Value *TranslationVisitor::operator()(minsts::Binary const *Inst) {
  return BinaryVisitorBase::visit(Inst);
}

llvm::Value *TranslationVisitor::getMemoryRWPtr(
    mir::Memory const &Memory, llvm::Value *Offset) {
  auto *InstancePtr = Context.getInstancePtr();
  auto *Address = Context.getLayout().get(Builder, InstancePtr, Memory);
  Address = Builder.CreatePtrToInt(Address, Builder.getIntPtrTy());
  if (Offset->getType() != Builder.getIntPtrTy())
    Offset = Builder.CreateZExtOrTrunc(Offset, Builder.getIntPtrTy());
  Address = Builder.CreateNUWAdd(Address, Offset);
  return Address;
}

llvm::Value *TranslationVisitor::operator()(minsts::Load const *Inst) {
  auto *MIRMemory = Inst->getLinearMemory();
  auto *Offset = Context[*Inst->getAddress()];
  auto *Address = getMemoryRWPtr(*MIRMemory, Offset);
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

llvm::Value *TranslationVisitor::operator()(minsts::Store const *Inst) {
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
  auto *Address = getMemoryRWPtr(*Inst->getLinearMemory(), Offset);
  auto *StoreTy = Value->getType();
  auto *StorePtrTy = llvm::PointerType::getUnqual(StoreTy);
  Address = Builder.CreateIntToPtr(Address, StorePtrTy);
  auto *Result = Builder.CreateStore(Value, Address);
  if (!Context.getLayout().getTranslationOptions().AssumeMemRWAligned)
    Result->setAlignment(llvm::Align(1));
  return Result;
}

llvm::Value *TranslationVisitor::operator()(minsts::MemoryGuard const *Inst) {
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

llvm::Value *TranslationVisitor::operator()(minsts::MemoryGrow const *Inst) {
  auto *InstancePtr = Context.getInstancePtr();
  auto *BuiltinMemoryGrow =
      Context.getLayout().getBuiltin("__sable_memory_grow");
  auto *Memory =
      Context.getLayout().get(Builder, InstancePtr, *Inst->getLinearMemory());
  auto *DeltaSize = Context[*Inst->getSize()];
  return Builder.CreateCall(BuiltinMemoryGrow, {Memory, DeltaSize});
}

llvm::Value *TranslationVisitor::operator()(minsts::MemorySize const *Inst) {
  auto *InstancePtr = Context.getInstancePtr();
  auto *BuiltinMemorySize =
      Context.getLayout().getBuiltin("__sable_memory_size");
  auto *Memory =
      Context.getLayout().get(Builder, InstancePtr, *Inst->getLinearMemory());
  return Builder.CreateCall(BuiltinMemorySize, {Memory});
}

llvm::Value *TranslationVisitor::operator()(minsts::Cast const *Inst) {
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

llvm::Value *TranslationVisitor::operator()(minsts::Extend const *Inst) {
  auto *Operand = Context[*Inst->getOperand()];
  auto OperandMIRTy = Context.getInferredType()[*Inst];
  auto *OperandTy = Context.getLayout().convertType(OperandMIRTy.asPrimitive());
  auto *FromTy = llvm::IntegerType::getIntNTy(
      Context.getTarget().getContext(), Inst->getFromWidth());
  llvm::Value *Result = Builder.CreateTrunc(Operand, FromTy);
  return Builder.CreateSExt(Result, OperandTy);
}

llvm::Value *TranslationVisitor::operator()(minsts::Pack const *Inst) {
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

llvm::Value *TranslationVisitor::operator()(minsts::Unpack const *Inst) {
  auto *Struct = Context[*Inst->getOperand()];
  auto Index = Inst->getIndex();
  return Builder.CreateExtractValue(Struct, Index);
}

llvm::Value *TranslationVisitor::operator()(minsts::Phi const *Inst) {
  auto *PhiTy = Context.getLayout().convertType(Inst->getType());
  auto NumCandidate = Inst->getNumCandidates();
  return Builder.CreatePHI(PhiTy, NumCandidate);
}

llvm::Value *TranslationVisitor::visit(mir::Instruction const *Inst) {
  auto *Result = InstVisitorBase::visit(Inst);
  if (Context.getInferredType()[*Inst].isPrimitiveI32() &&
      (Result->getType() == Builder.getInt1Ty())) {
    Result = Builder.CreateZExt(Result, Builder.getInt32Ty());
  }
  return Result;
}
} // namespace codegen::llvm_instance