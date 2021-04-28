#include "TranslationVisitor.h"

#include "LLVMCodegen.h"
#include "TranslationContext.h"

#include <range/v3/algorithm/fill_n.hpp>

#include <limits>

namespace codegen::llvm_instance {
using CastOpcode = mir::instructions::CastOpcode;

#define SABLE_CODEGEN_CAST(CAST_OPCODE)                                        \
  template <>                                                                  \
  llvm::Value *TranslationVisitor::codegenCast<CastOpcode::CAST_OPCODE>(       \
      llvm::Value * Operand)

// clang-format off
SABLE_CODEGEN_CAST(I32WrapI64) 
{ return Builder.CreateTrunc(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I32TruncF32S) 
{ return Builder.CreateFPToSI(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I32TruncF32U)
{ return Builder.CreateFPToUI(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I32TruncF64S)
{ return Builder.CreateFPToSI(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I32TruncF64U)
{ return Builder.CreateFPToUI(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I64ExtendI32S)
{ return Builder.CreateSExt(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(I64ExtendI32U)
{ return Builder.CreateZExt(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(I64TruncF32S)
{ return Builder.CreateFPToSI(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(I64TruncF32U)
{ return Builder.CreateFPToUI(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(I64TruncF64S)
{ return Builder.CreateFPToSI(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(I64TruncF64U)
{ return Builder.CreateFPToUI(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(F32ConvertI32S)
{ return Builder.CreateSIToFP(Operand, Builder.getFloatTy()); }
SABLE_CODEGEN_CAST(F32ConvertI32U)
{ return Builder.CreateUIToFP(Operand, Builder.getFloatTy()); }
SABLE_CODEGEN_CAST(F32ConvertI64S)
{ return Builder.CreateSIToFP(Operand, Builder.getFloatTy()); }
SABLE_CODEGEN_CAST(F32ConvertI64U)
{ return Builder.CreateUIToFP(Operand, Builder.getFloatTy()); }
SABLE_CODEGEN_CAST(F32DemoteF64)
{ return Builder.CreateFPTrunc(Operand, Builder.getFloatTy()); }
SABLE_CODEGEN_CAST(F64ConvertI32S)
{ return Builder.CreateSIToFP(Operand, Builder.getDoubleTy()); }
SABLE_CODEGEN_CAST(F64ConvertI32U)
{ return Builder.CreateUIToFP(Operand, Builder.getDoubleTy()); }
SABLE_CODEGEN_CAST(F64ConvertI64S)
{ return Builder.CreateSIToFP(Operand, Builder.getDoubleTy()); }
SABLE_CODEGEN_CAST(F64ConvertI64U)
{ return Builder.CreateUIToFP(Operand, Builder.getDoubleTy()); }
SABLE_CODEGEN_CAST(F64PromoteF32)
{ return Builder.CreateFPExt(Operand, Builder.getDoubleTy()); }
SABLE_CODEGEN_CAST(I32ReinterpretF32)
{ return Builder.CreateBitCast(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I64ReinterpretF64)
{ return Builder.CreateBitCast(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(F32ReinterpretI32)
{ return Builder.CreateBitCast(Operand, Builder.getFloatTy()); }
SABLE_CODEGEN_CAST(F64ReinterpretI64)
{ return Builder.CreateBitCast(Operand, Builder.getDoubleTy()); }
// clang-format on

// clang-format off
SABLE_CODEGEN_CAST(I32TruncSatF32S)
{ return Builder.CreateIntrinsicFPTruncSatS(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I32TruncSatF32U)
{ return Builder.CreateIntrinsicFPTruncSatU(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I32TruncSatF64S)
{ return Builder.CreateIntrinsicFPTruncSatS(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I32TruncSatF64U)
{ return Builder.CreateIntrinsicFPTruncSatU(Operand, Builder.getInt32Ty()); }
SABLE_CODEGEN_CAST(I64TruncSatF32S)
{ return Builder.CreateIntrinsicFPTruncSatS(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(I64TruncSatF32U)
{ return Builder.CreateIntrinsicFPTruncSatU(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(I64TruncSatF64S)
{ return Builder.CreateIntrinsicFPTruncSatS(Operand, Builder.getInt64Ty()); }
SABLE_CODEGEN_CAST(I64TruncSatF64U)
{ return Builder.CreateIntrinsicFPTruncSatU(Operand, Builder.getInt64Ty()); }
// clang-format on

namespace {
template <unsigned TruncTo, unsigned ExtTo>
llvm::Value *CreateTruncThenSExt(IRBuilder &Builder, llvm::Value *Operand) {
  Operand = Builder.CreateTrunc(Operand, Builder.getIntNTy(TruncTo));
  Operand = Builder.CreateSExt(Operand, Builder.getIntNTy(ExtTo));
  return Operand;
}
} // namespace

// clang-format off
SABLE_CODEGEN_CAST(I32Extend8S) 
{ return CreateTruncThenSExt< 8, 32>(Builder, Operand); }
SABLE_CODEGEN_CAST(I32Extend16S) 
{ return CreateTruncThenSExt<16, 32>(Builder, Operand); }
SABLE_CODEGEN_CAST(I64Extend8S) 
{ return CreateTruncThenSExt< 8, 64>(Builder, Operand); }
SABLE_CODEGEN_CAST(I64Extend16S) 
{ return CreateTruncThenSExt<16, 64>(Builder, Operand); }
SABLE_CODEGEN_CAST(I64Extend32S) 
{ return CreateTruncThenSExt<32, 64>(Builder, Operand); }
// clang-format on

namespace {
template <typename T>
void adjustSIMD128Operand(
    IRBuilder &Builder, llvm::Value *Operand, T const &LaneInfo) {
  auto *ExpectTy = Builder.getV128Ty(LaneInfo);
  if (Operand->getType() != ExpectTy)
    Operand = Builder.CreateBitCast(Operand, ExpectTy);
}
} // namespace

SABLE_CODEGEN_CAST(F32x4ConvertI32x4S) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI32x4());
  return Builder.CreateSIToFP(Operand, Builder.getV128F32x4());
}

SABLE_CODEGEN_CAST(F32x4ConvertI32x4U) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI32x4());
  return Builder.CreateUIToFP(Operand, Builder.getV128F32x4());
}

SABLE_CODEGEN_CAST(F64x2ConvertLowI32x4S) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI32x4());
  Operand = Builder.CreateVectorSliceLow(Operand);
  return Builder.CreateSIToFP(Operand, Builder.getV128F64x2());
}

SABLE_CODEGEN_CAST(F64x2ConvertLowI32x4U) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI32x4());
  Operand = Builder.CreateVectorSliceLow(Operand);
  return Builder.CreateUIToFP(Operand, Builder.getV128F64x2());
}

SABLE_CODEGEN_CAST(I32x4TruncSatF32x4S) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128FPLaneInfo::getF32x4());
  return Builder.CreateIntrinsicFPTruncSatS(Operand, Builder.getV128I32x4());
}

SABLE_CODEGEN_CAST(I32x4TruncSatF32x4U) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128FPLaneInfo::getF32x4());
  return Builder.CreateIntrinsicFPTruncSatU(Operand, Builder.getV128I32x4());
}

SABLE_CODEGEN_CAST(I32x4TruncSatF64x2SZero) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128FPLaneInfo::getF64x2());
  auto *I32x2Ty = llvm::FixedVectorType::get(Builder.getInt32Ty(), 2);
  auto *Result = Builder.CreateIntrinsicFPTruncSatS(Operand, I32x2Ty);
  auto *ZeroX2Vector =
      llvm::ConstantVector::get({Builder.getInt32(0), Builder.getInt32(0)});
  auto *IdentityVector = llvm::ConstantVector::get(
      {Builder.getInt32(0), Builder.getInt32(1), Builder.getInt32(2),
       Builder.getInt32(3)});
  return Builder.CreateShuffleVector(Result, ZeroX2Vector, IdentityVector);
}

SABLE_CODEGEN_CAST(I32x4TruncSatF64x2UZero) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128FPLaneInfo::getF64x2());
  auto *I32x2Ty = llvm::FixedVectorType::get(Builder.getInt32Ty(), 2);
  auto *Result = Builder.CreateIntrinsicFPTruncSatU(Operand, I32x2Ty);
  auto *ZeroX2Vector =
      llvm::ConstantVector::get({Builder.getInt32(0), Builder.getInt32(0)});
  auto *IdentityVector = llvm::ConstantVector::get(
      {Builder.getInt32(0), Builder.getInt32(1), Builder.getInt32(2),
       Builder.getInt32(3)});
  return Builder.CreateShuffleVector(Result, ZeroX2Vector, IdentityVector);
}

SABLE_CODEGEN_CAST(F32x4DemoteF64x2Zero) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128FPLaneInfo::getF64x2());
  auto *F32x2Ty = llvm::FixedVectorType::get(Builder.getFloatTy(), 2);
  auto *Result = Builder.CreateFPTrunc(Operand, F32x2Ty);
  auto *ZeroX2Vector = llvm::ConstantVector::get(
      {Builder.getFloat(0.0f), Builder.getFloat(0.0f)});
  auto *IdentityVector = llvm::ConstantVector::get(
      {Builder.getInt32(0), Builder.getInt32(1), Builder.getInt32(2),
       Builder.getInt32(3)});
  return Builder.CreateShuffleVector(Result, ZeroX2Vector, IdentityVector);
}

SABLE_CODEGEN_CAST(F64x2PromoteLowF32x4) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128FPLaneInfo::getF32x4());
  Operand = Builder.CreateVectorSliceLow(Operand);
  return Builder.CreateFPExt(Operand, Builder.getV128F64x2());
}

SABLE_CODEGEN_CAST(I16x8ExtendLowI8x16S) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI8x16());
  Operand = Builder.CreateVectorSliceLow(Operand);
  return Builder.CreateSExt(Operand, Builder.getV128I16x8());
}

SABLE_CODEGEN_CAST(I16x8ExtendHighI8x16S) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI8x16());
  Operand = Builder.CreateVectorSliceHigh(Operand);
  return Builder.CreateSExt(Operand, Builder.getV128I16x8());
}

SABLE_CODEGEN_CAST(I16x8ExtendLowI8x16U) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI8x16());
  Operand = Builder.CreateVectorSliceLow(Operand);
  return Builder.CreateZExt(Operand, Builder.getV128I16x8());
}

SABLE_CODEGEN_CAST(I16x8ExtendHighI8x16U) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI8x16());
  Operand = Builder.CreateVectorSliceHigh(Operand);
  return Builder.CreateZExt(Operand, Builder.getV128I16x8());
}

SABLE_CODEGEN_CAST(I32x4ExtendLowI16x8S) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI16x8());
  Operand = Builder.CreateVectorSliceLow(Operand);
  return Builder.CreateSExt(Operand, Builder.getV128I32x4());
}

SABLE_CODEGEN_CAST(I32x4ExtendHighI16x8S) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI16x8());
  Operand = Builder.CreateVectorSliceHigh(Operand);
  return Builder.CreateSExt(Operand, Builder.getV128I32x4());
}

SABLE_CODEGEN_CAST(I32x4ExtendLowI16x8U) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI16x8());
  Operand = Builder.CreateVectorSliceLow(Operand);
  return Builder.CreateZExt(Operand, Builder.getV128I32x4());
}

SABLE_CODEGEN_CAST(I32x4ExtendHighI16x8U) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI16x8());
  Operand = Builder.CreateVectorSliceHigh(Operand);
  return Builder.CreateZExt(Operand, Builder.getV128I32x4());
}

SABLE_CODEGEN_CAST(I64x2ExtendLowI32x4S) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI32x4());
  Operand = Builder.CreateVectorSliceLow(Operand);
  return Builder.CreateSExt(Operand, Builder.getV128I64x2());
}

SABLE_CODEGEN_CAST(I64x2ExtendHighI32x4S) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI32x4());
  Operand = Builder.CreateVectorSliceHigh(Operand);
  return Builder.CreateSExt(Operand, Builder.getV128I64x2());
}

SABLE_CODEGEN_CAST(I64x2ExtendLowI32x4U) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI32x4());
  Operand = Builder.CreateVectorSliceLow(Operand);
  return Builder.CreateZExt(Operand, Builder.getV128I64x2());
}

SABLE_CODEGEN_CAST(I64x2ExtendHighI32x4U) {
  adjustSIMD128Operand(Builder, Operand, mir::SIMD128IntLaneInfo::getI32x4());
  Operand = Builder.CreateVectorSliceHigh(Operand);
  return Builder.CreateZExt(Operand, Builder.getV128I64x2());
}

llvm::Value *
TranslationVisitor::operator()(mir::instructions::Cast const *Inst) {
  auto *Operand = Context[*Inst->getOperand()];
  switch (Inst->getCastOpcode()) {
#define X(Name, ...)                                                           \
  case CastOpcode::Name:                                                       \
    return codegenCast<CastOpcode::Name>(Operand);
#include "../mir/Cast.defs"
#undef X
  default: utility::unreachable();
  }
}
} // namespace codegen::llvm_instance