#include "LLVMCodegen.h"

#include <llvm/IR/Intrinsics.h>

namespace codegen::llvm_instance {

IRBuilder::IRBuilder(llvm::Module &EnclosingModule_)
    : llvm::IRBuilder<>(EnclosingModule_.getContext()),
      EnclosingModule(std::addressof(EnclosingModule_)) {}

IRBuilder::IRBuilder(llvm::BasicBlock &BasicBlock_)
    : llvm::IRBuilder<>(std::addressof(BasicBlock_)),
      EnclosingModule(BasicBlock_.getParent()->getParent()) {}

IRBuilder::IRBuilder(llvm::Function &Function_)
    : llvm::IRBuilder<>(Function_.getContext()),
      EnclosingModule(Function_.getParent()) {}

// clang-format off
llvm::PointerType *IRBuilder::getInt32PtrTy(unsigned AddressSpace)
{ return llvm::PointerType::get(getInt32Ty(), AddressSpace); }
llvm::PointerType *IRBuilder::getInt64PtrTy(unsigned AddressSpace)
{ return llvm::PointerType::get(getInt64Ty(), AddressSpace); }
llvm::PointerType *IRBuilder::getFloatPtrTy(unsigned AddressSpace)
{ return llvm::PointerType::get(getFloatTy(), AddressSpace); }
llvm::PointerType *IRBuilder::getDoublePtrTy(unsigned AddressSpace)
{ return llvm::PointerType::get(getDoubleTy(), AddressSpace); }
llvm::Constant *IRBuilder::getFloat(float Value)
{ return llvm::ConstantFP::get(getFloatTy(), Value); }
llvm::Constant *IRBuilder::getDouble(double Value)
{ return llvm::ConstantFP::get(getDoubleTy(), Value); }
// clang-format on

llvm::Type *IRBuilder::getCStrTy(unsigned AddressSpace) {
  return getInt8PtrTy(AddressSpace);
}

llvm::Constant *
IRBuilder::getCStr(std::string_view String, std::string_view Name) {
  auto &Context = getContext();
  auto *StringConstant = llvm::ConstantDataArray::getString(Context, String);
  auto *StringGlobal = new llvm::GlobalVariable(
      /* Parent      */ *EnclosingModule,
      /* Type        */ StringConstant->getType(),
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::PrivateLinkage,
      /* Initializer */ StringConstant);
  StringGlobal->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
  StringGlobal->setAlignment(llvm::Align(1));
  StringGlobal->setName(llvm::StringRef(Name));
  auto *Zero = getInt32(0);
  std::array<llvm::Constant *, 2> Indices{Zero, Zero};
  auto *CastedPtr = llvm::dyn_cast<llvm::GlobalVariable>(StringGlobal);
  return llvm::ConstantExpr::getInBoundsGetElementPtr(
      CastedPtr->getValueType(), StringGlobal, Indices);
}

llvm::Type *IRBuilder::getIntPtrTy(unsigned AddressSpace) {
  auto const &DataLayout = EnclosingModule->getDataLayout();
  return llvm::IRBuilder<>::getIntPtrTy(DataLayout, AddressSpace);
}

llvm::VectorType *
IRBuilder::getV128Ty(mir::SIMD128IntLaneInfo const &LaneInfo) {
  auto LaneWidth = LaneInfo.getLaneWidth();
  auto NumLane = LaneInfo.getNumLane();
  auto *ElementTy = getIntNTy(LaneWidth);
  return llvm::FixedVectorType::get(ElementTy, NumLane);
}

llvm::VectorType *IRBuilder::getV128Ty(mir::SIMD128FPLaneInfo const &LaneInfo) {
  auto LaneWidth = LaneInfo.getLaneWidth();
  auto NumLane = LaneInfo.getNumLane();
  llvm::Type *ElementTy = nullptr;
  switch (LaneWidth) {
  case 32: ElementTy = getFloatTy(); break;
  case 64: ElementTy = getDoubleTy(); break;
  default: utility::unreachable();
  }
  return llvm::FixedVectorType::get(ElementTy, NumLane);
}

llvm::Constant *IRBuilder::getV128(
    bytecode::V128Value const &Value, mir::SIMD128IntLaneInfo const &LaneInfo) {
  switch (LaneInfo.getElementKind()) {
  case mir::SIMD128IntElementKind::I8: {
    std::array<llvm::Constant *, 16> Elements;
    auto ValueView = Value.asI8x16();
    for (std::size_t I = 0; I < 16; ++I) Elements[I] = getInt8(ValueView[I]);
    return llvm::ConstantVector::get(Elements);
  }
  case mir::SIMD128IntElementKind::I16: {
    std::array<llvm::Constant *, 8> Elements;
    auto ValueView = Value.asI16x8();
    for (std::size_t I = 0; I < 8; ++I) Elements[I] = getInt16(ValueView[I]);
    return llvm::ConstantVector::get(Elements);
  }
  case mir::SIMD128IntElementKind::I32: {
    std::array<llvm::Constant *, 4> Elements;
    auto ValueView = Value.asI32x4();
    for (std::size_t I = 0; I < 4; ++I) Elements[I] = getInt32(ValueView[I]);
    return llvm::ConstantVector::get(Elements);
  }
  case mir::SIMD128IntElementKind::I64: {
    std::array<llvm::Constant *, 2> Elements;
    auto ValueView = Value.asI64x2();
    for (std::size_t I = 0; I < 2; ++I) Elements[I] = getInt64(ValueView[I]);
    return llvm::ConstantVector::get(Elements);
  }
  default: utility::unreachable();
  }
}

llvm::Constant *IRBuilder::getV128(
    bytecode::V128Value const &Value, mir::SIMD128FPLaneInfo const &LaneInfo) {
  switch (LaneInfo.getElementKind()) {
  case mir::SIMD128FPElementKind::F32: {
    std::array<llvm::Constant *, 4> Elements;
    auto ValueView = Value.asF32x4();
    for (std::size_t I = 0; I < 4; ++I) Elements[I] = getFloat(ValueView[I]);
    return llvm::ConstantVector::get(Elements);
  }
  case mir::SIMD128FPElementKind::F64: {
    std::array<llvm::Constant *, 4> Elements;
    auto ValueView = Value.asF64x2();
    for (std::size_t I = 0; I < 2; ++I) Elements[I] = getFloat(ValueView[I]);
    return llvm::ConstantVector::get(Elements);
  }
  default: utility::unreachable();
  }
}

llvm::Value *IRBuilder::CreateIntrinsicClz(llvm::Value *Operand) {
  assert(Operand->getType()->isIntOrIntVectorTy());
  std::array<llvm::Type *, 2> IntrinsicArgTys{Operand->getType(), getInt1Ty()};
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      EnclosingModule, llvm::Intrinsic::ctlz, IntrinsicArgTys);
  return CreateCall(Intrinsic, {Operand, getFalse()});
}

llvm::Value *IRBuilder::CreateIntrinsicCtz(llvm::Value *Operand) {
  assert(Operand->getType()->isIntOrIntVectorTy());
  std::array<llvm::Type *, 2> IntrinsicArgTys{Operand->getType(), getInt1Ty()};
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      EnclosingModule, llvm::Intrinsic::cttz, IntrinsicArgTys);
  return CreateCall(Intrinsic, {Operand, getFalse()});
}

namespace {
template <llvm::Intrinsic::ID IntrinsicID>
llvm::Value *CreateIntUnaryIntrinsic(IRBuilder &Builder, llvm::Value *Operand) {
  assert(Operand->getType()->isIntOrIntVectorTy());
  std::array<llvm::Type *, 1> IntrinsicArgTys{Operand->getType()};
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      std::addressof(Builder.getModule()), IntrinsicID, IntrinsicArgTys);
  return Builder.CreateCall(Intrinsic, {Operand});
}

constexpr auto popcnt = llvm::Intrinsic::ctpop;
constexpr auto vector_reduce_add = llvm::Intrinsic::vector_reduce_add;
} // namespace

// clang-format off
llvm::Value *IRBuilder::CreateIntrinsicPopcnt(llvm::Value *Operand) 
{ return CreateIntUnaryIntrinsic<popcnt>(*this, Operand); }
llvm::Value *IRBuilder::CreateIntrinsicReduceAnd(llvm::Value *Operand) 
{ return CreateIntUnaryIntrinsic<vector_reduce_add>(*this, Operand); }
// clang-format on

namespace {
template <llvm::Intrinsic::ID IntrinsicID>
llvm::Value *CreateFPUnaryIntrinsic(IRBuilder &Builder, llvm::Value *Operand) {
  assert(Operand->getType()->isFPOrFPVectorTy());
  std::array<llvm::Type *, 1> IntrinsicArgTys{Operand->getType()};
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      std::addressof(Builder.getModule()), IntrinsicID, IntrinsicArgTys);
  return Builder.CreateCall(Intrinsic, {Operand});
}
} // namespace

// clang-format off
llvm::Value *IRBuilder::CreateIntrinsicFPAbs(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<llvm::Intrinsic::fabs>(*this, Operand); }
llvm::Value *IRBuilder::CreateIntrinsicCeil(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<llvm::Intrinsic::ceil>(*this, Operand); }
llvm::Value *IRBuilder::CreateIntrinsicFloor(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<llvm::Intrinsic::floor>(*this, Operand); }
llvm::Value * IRBuilder::CreateIntrinsicTrunc(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<llvm::Intrinsic::trunc>(*this, Operand); }
llvm::Value * IRBuilder::CreateIntrinsicNearest(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<llvm::Intrinsic::nearbyint>(*this, Operand); }
llvm::Value * IRBuilder::CreateIntrinsicSqrt(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<llvm::Intrinsic::sqrt>(*this, Operand); }
// clang-format on
} // namespace codegen::llvm_instance