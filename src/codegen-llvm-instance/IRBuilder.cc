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

// clang-format off
llvm::VectorType *IRBuilder::getV128I8x16() 
{ return getV128Ty(mir::SIMD128IntLaneInfo(mir::SIMD128IntElementKind::I8)); }
llvm::VectorType *IRBuilder::getV128I16x8()
{ return getV128Ty(mir::SIMD128IntLaneInfo(mir::SIMD128IntElementKind::I16)); }
llvm::VectorType *IRBuilder::getV128I32x4()
{ return getV128Ty(mir::SIMD128IntLaneInfo(mir::SIMD128IntElementKind::I32)); }
llvm::VectorType *IRBuilder::getV128I64x2()
{ return getV128Ty(mir::SIMD128IntLaneInfo(mir::SIMD128IntElementKind::I64)); }
llvm::VectorType *IRBuilder::getV128F32x4()
{ return getV128Ty(mir::SIMD128FPLaneInfo(mir::SIMD128FPElementKind::F32)); }
llvm::VectorType *IRBuilder::getV128F64x2()
{ return getV128Ty(mir::SIMD128FPLaneInfo(mir::SIMD128FPElementKind::F64)); }
// clang-format on

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
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      EnclosingModule, llvm::Intrinsic::ctlz,
      {Operand->getType(), getInt1Ty()});
  return CreateCall(Intrinsic, {Operand, getFalse()});
}

llvm::Value *IRBuilder::CreateIntrinsicCtz(llvm::Value *Operand) {
  assert(Operand->getType()->isIntOrIntVectorTy());
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      EnclosingModule, llvm::Intrinsic::cttz,
      {Operand->getType(), getInt1Ty()});
  return CreateCall(Intrinsic, {Operand, getFalse()});
}

namespace {
template <llvm::Intrinsic::ID IntrinsicID>
llvm::Value *CreateIntUnaryIntrinsic(IRBuilder &Builder, llvm::Value *Operand) {
  assert(Operand->getType()->isIntOrIntVectorTy());
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      std::addressof(Builder.getModule()), IntrinsicID, {Operand->getType()});
  return Builder.CreateCall(Intrinsic, {Operand});
}

constexpr auto popcnt = llvm::Intrinsic::ctpop;
constexpr auto int_abs = llvm::Intrinsic::abs;
} // namespace

// clang-format off
llvm::Value *IRBuilder::CreateIntrinsicPopcnt(llvm::Value *Operand) 
{ return CreateIntUnaryIntrinsic<popcnt>(*this, Operand); }
llvm::Value *IRBuilder::CreateIntrinsicIntAbs(llvm::Value *Operand)
{ return CreateIntUnaryIntrinsic<int_abs>(*this, Operand); }
// clang-format on

namespace {
template <llvm::Intrinsic::ID IntrinsicID>
llvm::Value *CreateIntBinaryIntrinsic(
    IRBuilder &Builder, llvm::Value *LHS, llvm::Value *RHS) {
  assert(LHS->getType()->isIntOrIntVectorTy());
  assert(RHS->getType()->isIntOrIntVectorTy());
  assert(LHS->getType() == RHS->getType());
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      std::addressof(Builder.getModule()), IntrinsicID,
      {LHS->getType(), RHS->getType()});
  return Builder.CreateCall(Intrinsic, {LHS, RHS});
}

constexpr auto add_sat_s = llvm::Intrinsic::sadd_sat;
constexpr auto add_sat_u = llvm::Intrinsic::uadd_sat;
constexpr auto sub_sat_s = llvm::Intrinsic::ssub_sat;
constexpr auto sub_sat_u = llvm::Intrinsic::usub_sat;
constexpr auto int_min_s = llvm::Intrinsic::smin;
constexpr auto int_min_u = llvm::Intrinsic::umin;
constexpr auto int_max_s = llvm::Intrinsic::smax;
constexpr auto int_max_u = llvm::Intrinsic::umax;
} // namespace

// clang-format off
llvm::Value *IRBuilder::CreateIntrinsicAddSatS
(llvm::Value *LHS, llvm::Value *RHS)
{ return CreateIntBinaryIntrinsic<add_sat_s>(*this, LHS, RHS); }
llvm::Value *IRBuilder::CreateIntrinsicAddSatU
(llvm::Value *LHS, llvm::Value *RHS)
{ return CreateIntBinaryIntrinsic<add_sat_u>(*this, LHS, RHS); }
llvm::Value *IRBuilder::CreateIntrinsicSubSatS
(llvm::Value *LHS, llvm::Value *RHS)
{ return CreateIntBinaryIntrinsic<sub_sat_s>(*this, LHS, RHS); }
llvm::Value *IRBuilder::CreateIntrinsicSubSatU
(llvm::Value *LHS, llvm::Value *RHS)
{ return CreateIntBinaryIntrinsic<sub_sat_u>(*this, LHS, RHS); }
llvm::Value *IRBuilder::CreateIntrinsicIntMinS
(llvm::Value *LHS, llvm::Value *RHS)
{ return CreateIntBinaryIntrinsic<int_min_s>(*this, LHS, RHS); }
llvm::Value *IRBuilder::CreateIntrinsicIntMinU
(llvm::Value *LHS, llvm::Value *RHS)
{ return CreateIntBinaryIntrinsic<int_min_u>(*this, LHS, RHS); }
llvm::Value *IRBuilder::CreateIntrinsicIntMaxS
(llvm::Value *LHS, llvm::Value *RHS)
{ return CreateIntBinaryIntrinsic<int_max_s>(*this, LHS, RHS); }
llvm::Value *IRBuilder::CreateIntrinsicIntMaxU
(llvm::Value *LHS, llvm::Value *RHS)
{ return CreateIntBinaryIntrinsic<int_max_u>(*this, LHS, RHS); }
// clang-format on

namespace {
template <llvm::Intrinsic::ID IntrinsicID>
llvm::Value *CreateFPUnaryIntrinsic(IRBuilder &Builder, llvm::Value *Operand) {
  assert(Operand->getType()->isFPOrFPVectorTy());
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      std::addressof(Builder.getModule()), IntrinsicID, {Operand->getType()});
  return Builder.CreateCall(Intrinsic, {Operand});
}

constexpr auto fp_abs = llvm::Intrinsic::fabs;
constexpr auto ceil = llvm::Intrinsic::ceil;
constexpr auto floor = llvm::Intrinsic::floor;
constexpr auto trunc = llvm::Intrinsic::trunc;
constexpr auto nearest = llvm::Intrinsic::nearbyint;
constexpr auto sqrt = llvm::Intrinsic::sqrt;
} // namespace

// clang-format off
llvm::Value *IRBuilder::CreateIntrinsicFPAbs(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<fp_abs>(*this, Operand); }
llvm::Value *IRBuilder::CreateIntrinsicCeil(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<ceil>(*this, Operand); }
llvm::Value *IRBuilder::CreateIntrinsicFloor(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<floor>(*this, Operand); }
llvm::Value * IRBuilder::CreateIntrinsicTrunc(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<trunc>(*this, Operand); }
llvm::Value * IRBuilder::CreateIntrinsicNearest(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<nearest>(*this, Operand); }
llvm::Value * IRBuilder::CreateIntrinsicSqrt(llvm::Value *Operand)
{ return CreateFPUnaryIntrinsic<sqrt>(*this, Operand); }
// clang-format on

namespace {
template <llvm::Intrinsic::ID IntrinsicID>
llvm::Value *CreateFPBinaryIntrinsic(
    IRBuilder &Builder, llvm::Value *LHS, llvm::Value *RHS) {
  assert(LHS->getType()->isFPOrFPVectorTy());
  assert(RHS->getType()->isFPOrFPVectorTy());
  assert(LHS->getType() == RHS->getType());
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      std::addressof(Builder.getModule()), IntrinsicID,
      {LHS->getType(), RHS->getType()});
  return Builder.CreateCall(Intrinsic, {LHS, RHS});
}

constexpr auto copysign = llvm::Intrinsic::copysign;
} // namespace

// clang-format off
llvm::Value *
IRBuilder::CreateIntrinsicCopysign(llvm::Value *LHS, llvm::Value *RHS)
{ return CreateFPBinaryIntrinsic<copysign>(*this, LHS, RHS); }
// clang-format on

llvm::Value *
IRBuilder::CreateIntrinsicFPTruncSatS(llvm::Value *Value, llvm::Type *ToType) {
  assert(ToType->isIntegerTy());
  assert(Value->getType()->isFloatingPointTy());
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      EnclosingModule, llvm::Intrinsic::fptosi_sat, {ToType, Value->getType()});
  return CreateCall(Intrinsic, {Value});
}

llvm::Value *
IRBuilder::CreateIntrinsicFPTruncSatU(llvm::Value *Value, llvm::Type *ToType) {
  assert(ToType->isIntegerTy());
  assert(Value->getType()->isFloatingPointTy());
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      EnclosingModule, llvm::Intrinsic::fptoui_sat, {ToType, Value->getType()});
  return CreateCall(Intrinsic, {Value});
}

llvm::Value *IRBuilder::CreateIntrinsicFShl(
    llvm::Value *LHS, llvm::Value *RHS, llvm::Value *ShiftAmount) {
  assert(LHS->getType()->isIntOrIntVectorTy());
  assert(RHS->getType()->isIntOrIntVectorTy());
  assert(ShiftAmount->getType()->isIntOrIntVectorTy());
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      EnclosingModule, llvm::Intrinsic::fshl,
      {LHS->getType(), RHS->getType(), ShiftAmount->getType()});
  return CreateCall(Intrinsic, {LHS, RHS, ShiftAmount});
}

llvm::Value *IRBuilder::CreateIntrinsicFShr(
    llvm::Value *LHS, llvm::Value *RHS, llvm::Value *ShiftAmount) {
  assert(LHS->getType()->isIntOrIntVectorTy());
  assert(RHS->getType()->isIntOrIntVectorTy());
  assert(ShiftAmount->getType()->isIntOrIntVectorTy());
  auto *Intrinsic = llvm::Intrinsic::getDeclaration(
      EnclosingModule, llvm::Intrinsic::fshr,
      {LHS->getType(), RHS->getType(), ShiftAmount->getType()});
  return CreateCall(Intrinsic, {LHS, RHS, ShiftAmount});
}

llvm::Value *IRBuilder::CreateVectorSliceLow(llvm::Value *Value) {
  assert(Value->getType()->isVectorTy());
  auto *CastedTy = llvm::dyn_cast<llvm::VectorType>(Value->getType());
  unsigned MidPointIndex = CastedTy->getElementCount().getValue() / 2;
  std::vector<llvm::Constant *> ShuffleIndices;
  ShuffleIndices.reserve(MidPointIndex);
  for (unsigned I = 0; I < MidPointIndex; ++I)
    ShuffleIndices.push_back(getInt32(I));
  auto *Undef = llvm::UndefValue::get(Value->getType());
  auto *ShuffleVector = llvm::ConstantVector::get(ShuffleIndices);
  return CreateShuffleVector(Value, Undef, ShuffleVector);
}

llvm::Value *IRBuilder::CreateVectorSliceHigh(llvm::Value *Value) {
  assert(Value->getType()->isVectorTy());
  auto *CastedTy = llvm::dyn_cast<llvm::VectorType>(Value->getType());
  unsigned MidPointIndex = CastedTy->getElementCount().getValue() / 2;
  unsigned VectorLength = CastedTy->getElementCount().getValue();
  std::vector<llvm::Constant *> ShuffleIndices;
  ShuffleIndices.reserve(VectorLength - MidPointIndex);
  for (unsigned I = MidPointIndex; I < VectorLength; ++I)
    ShuffleIndices.push_back(getInt32(I));
  auto *Undef = llvm::UndefValue::get(Value->getType());
  auto *ShuffleVector = llvm::ConstantVector::get(ShuffleIndices);
  return CreateShuffleVector(Value, Undef, ShuffleVector);
}

llvm::Value *IRBuilder::CreateVectorSliceOdd(llvm::Value *Value) {
  assert(Value->getType()->isVectorTy());
  auto *CastedTy = llvm::dyn_cast<llvm::VectorType>(Value->getType());
  unsigned VectorLength = CastedTy->getElementCount().getValue();
  std::vector<llvm::Constant *> ShuffleIndices;
  ShuffleIndices.reserve(VectorLength / 2);
  for (unsigned I = 1; I < VectorLength; I = I + 2)
    ShuffleIndices.push_back(getInt32(I));
  auto *Undef = llvm::UndefValue::get(Value->getType());
  auto *ShuffleVector = llvm::ConstantVector::get(ShuffleIndices);
  return CreateShuffleVector(Value, Undef, ShuffleVector);
}

llvm::Value *IRBuilder::CreateVectorSliceEven(llvm::Value *Value) {
  assert(Value->getType()->isVectorTy());
  auto *CastedTy = llvm::dyn_cast<llvm::VectorType>(Value->getType());
  unsigned VectorLength = CastedTy->getElementCount().getValue();
  std::vector<llvm::Constant *> ShuffleIndices;
  ShuffleIndices.reserve(VectorLength / 2);
  for (unsigned I = 0; I < VectorLength; I = I + 2)
    ShuffleIndices.push_back(getInt32(I));
  auto *Undef = llvm::UndefValue::get(Value->getType());
  auto *ShuffleVector = llvm::ConstantVector::get(ShuffleIndices);
  return CreateShuffleVector(Value, Undef, ShuffleVector);
}
} // namespace codegen::llvm_instance