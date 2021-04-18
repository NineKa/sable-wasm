#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_LLVM_CODEGEN
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_LLVM_CODEGEN

#include "../bytecode/Module.h"
#include "../mir/Module.h"
#include "TranslationContext.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringSet.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <string_view>

namespace codegen::llvm_instance {

struct TranslationOptions {
  bool SkipMemBoundaryCheck = false;
  bool SkipTblBoundaryCheck = false;
  bool AssumeMemRWAligned = false;
};

class IRBuilder : public llvm::IRBuilder<> {
  llvm::Module *EnclosingModule;

public:
  explicit IRBuilder(llvm::Module &EnclosingModule_);
  explicit IRBuilder(llvm::BasicBlock &BasicBlock_);
  explicit IRBuilder(llvm::Function &Function_);

  llvm::Module &getModule() { return *EnclosingModule; }

  llvm::PointerType *getInt32PtrTy(unsigned AddressSpace = 0);
  llvm::PointerType *getInt64PtrTy(unsigned AddressSpace = 0);
  llvm::PointerType *getFloatPtrTy(unsigned AddressSpace = 0);
  llvm::PointerType *getDoublePtrTy(unsigned AddressSpace = 0);
  llvm::Constant *getFloat(float Value);
  llvm::Constant *getDouble(double Value);

  llvm::Type *getCStrTy(unsigned AddressSpace = 0);
  llvm::Constant *getCStr(std::string_view String, std::string_view Name = "");

  llvm::Type *getIntPtrTy(unsigned AddressSpace = 0);

  llvm::VectorType *getV128Ty(mir::SIMD128IntLaneInfo const &LaneInfo);
  llvm::VectorType *getV128Ty(mir::SIMD128FPLaneInfo const &LaneInfo);
  llvm::VectorType *getV128I8x16();
  llvm::VectorType *getV128I16x8();
  llvm::VectorType *getV128I32x4();
  llvm::VectorType *getV128I64x2();
  llvm::VectorType *getV128F32x4();
  llvm::VectorType *getV128F64x2();

  llvm::Constant *getV128(
      bytecode::V128Value const &Value,
      mir::SIMD128IntLaneInfo const &LaneInfo =
          mir::SIMD128IntLaneInfo(mir::SIMD128IntElementKind::I8));

  llvm::Constant *getV128(
      bytecode::V128Value const &Value, mir::SIMD128FPLaneInfo const &LaneInfo);

  llvm::Value *CreateIntrinsicClz(llvm::Value *Operand);
  llvm::Value *CreateIntrinsicCtz(llvm::Value *Operand);
  llvm::Value *CreateIntrinsicPopcnt(llvm::Value *Operand);
  llvm::Value *CreateIntrinsicIntAbs(llvm::Value *Operand);

  llvm::Value *CreateIntrinsicAddSatS(llvm::Value *LHS, llvm::Value *RHS);
  llvm::Value *CreateIntrinsicAddSatU(llvm::Value *LHS, llvm::Value *RHS);
  llvm::Value *CreateIntrinsicSubSatS(llvm::Value *LHS, llvm::Value *RHS);
  llvm::Value *CreateIntrinsicSubSatU(llvm::Value *LHS, llvm::Value *RHS);
  llvm::Value *CreateIntrinsicIntMinS(llvm::Value *LHS, llvm::Value *RHS);
  llvm::Value *CreateIntrinsicIntMinU(llvm::Value *LHS, llvm::Value *RHS);
  llvm::Value *CreateIntrinsicIntMaxS(llvm::Value *LHS, llvm::Value *RHS);
  llvm::Value *CreateIntrinsicIntMaxU(llvm::Value *LHS, llvm::Value *RHS);

  llvm::Value *CreateIntrinsicFPAbs(llvm::Value *Operand);
  llvm::Value *CreateIntrinsicCeil(llvm::Value *Operand);
  llvm::Value *CreateIntrinsicFloor(llvm::Value *Operand);
  llvm::Value *CreateIntrinsicTrunc(llvm::Value *Operand);
  llvm::Value *CreateIntrinsicNearest(llvm::Value *Operand);
  llvm::Value *CreateIntrinsicSqrt(llvm::Value *Operand);

  llvm::Value *CreateIntrinsicCopysign(llvm::Value *LHS, llvm::Value *RHS);

  llvm::Value *
  CreateIntrinsicFPTruncSatS(llvm::Value *Value, llvm::Type *ToType);
  llvm::Value *
  CreateIntrinsicFPTruncSatU(llvm::Value *Value, llvm::Type *ToType);

  // clang-format off
  llvm::Value *CreateIntrinsicFShl
  (llvm::Value *LHS, llvm::Value *RHS, llvm::Value *ShiftAmount);
  llvm::Value *CreateIntrinsicFShr
  (llvm::Value *LHS, llvm::Value *RHS, llvm::Value *ShiftAmount);
  // clang-format on

  llvm::Value *CreateVectorSliceLow(llvm::Value *Value);
  llvm::Value *CreateVectorSliceHigh(llvm::Value *Value);
  llvm::Value *CreateVectorSliceOdd(llvm::Value *Value);
  llvm::Value *CreateVectorSliceEven(llvm::Value *Value);
};

class EntityLayout {
public:
  class FunctionEntry {
    std::size_t Index;
    llvm::Function *Definition;
    llvm::Constant *Signature;

  public:
    FunctionEntry(
        std::size_t Index_, llvm::Function *Definition_,
        llvm::Constant *Signature_)
        : Index(Index_), Definition(Definition_), Signature(Signature_) {}
    std::size_t index() const { return Index; }
    llvm::Function *definition() const { return Definition; }
    llvm::Constant *signature() const { return Signature; }
  };

private:
  mir::Module const &Source;
  llvm::Module &Target;
  TranslationOptions Options;
  mutable IRBuilder ModuleIRBuilder;

  llvm::StringMap<llvm::Type *> NamedStructTys;
  llvm::StringMap<llvm::Type *> NamedOpaqueTys;

  llvm::DenseMap<mir::ASTNode const *, std::size_t> OffsetMap;
  llvm::DenseMap<mir::Data const *, llvm::Constant *> DataMap;
  llvm::DenseMap<mir::Element const *, llvm::Constant *> ElementMap;
  llvm::DenseMap<mir::Function const *, FunctionEntry> FunctionMap;

  llvm::StructType *declareOpaqueTy(std::string_view Name);
  llvm::StructType *getOpaqueTy(std::string_view Name) const;
  llvm::StructType *createNamedStructTy(std::string_view Name);
  llvm::StructType *getNamedStructTy(std::string_view Name) const;

  /*
   * Instance Struct Layout:
   * __sable_memory_metadata_t *
   * __sable_table_metadata_t *
   * __sable_global_metadata_t *
   * __sable_function_metadata_t *
   * ... Memory Instance Pointers (__sable_memory_t *)
   * ... Table Instance Pointers  (__sable_table_t *)
   * ... Global Instance Pointers (__sable_global_t *)
   * ... Function Pointers        (__sable_instance_t *, __sable_function_t *)
   */

  void setupInstanceType();

  llvm::Value *translateInitExpr(
      IRBuilder &Builder, llvm::Value *InstancePtr,
      mir::InitializerExpr const &Expr);

  void setupDataSegments();
  void setupElementSegments();

  llvm::GlobalVariable *createArrayGlobal(
      llvm::Type *ElementType, std::span<llvm::Constant *const> Elements);
  llvm::GlobalVariable *createMetadata(
      std::string_view Prefix, std::uint32_t Size, std::uint32_t ImportSize,
      std::uint32_t ExportSize, llvm::GlobalVariable *Entities,
      llvm::GlobalVariable *Imports, llvm::GlobalVariable *Exports);

  void setupMemoryMetadata();
  void setupTableMetadata();
  void setupGlobalMetadata();
  void setupFunctionMetadata();
  void setupFunctions();
  void setupInitializer();
  void setupBuiltins();

  std::size_t getOffset(mir::ASTNode const &Node) const;

public:
  EntityLayout(
      mir::Module const &Source_, llvm::Module &Target_,
      TranslationOptions Options);

  TranslationOptions const &getTranslationOptions() const;

  llvm::Type *convertType(bytecode::ValueType const &Type) const;
  llvm::FunctionType *convertType(bytecode::FunctionType const &Type) const;

  llvm::Constant *operator[](mir::Data const &DataSegment) const;
  FunctionEntry const &operator[](mir::Function const &Function) const;
  llvm::Constant *operator[](mir::Element const &ElementSegment) const;

  /* List of Builtins (implement by the runtime library
   * __sable_memory_guard
   * __sable_table_guard
   * __sable_table_set
   * __sable_table_check
   * __sable_table_function    (* no boundary check is required *)
   * __sable_table_context     (* no boundary check is required *)
   * error handling:
   * __sable_unreachable
   */
  llvm::Function *getBuiltin(std::string_view Name) const;

  llvm::Value *get(IRBuilder &, llvm::Value *, mir::Global const &) const;
  llvm::Value *get(IRBuilder &, llvm::Value *, mir::Memory const &) const;
  llvm::Value *get(IRBuilder &, llvm::Value *, mir::Table const &) const;

  llvm::Value *
  getContextPtr(IRBuilder &, llvm::Value *, mir::Function const &) const;
  llvm::Value *
  getFunctionPtr(IRBuilder &, llvm::Value *, mir::Function const &) const;

  char getSignature(bytecode::ValueType const &Type) const;
  char getSignature(bytecode::GlobalType const &Type) const;
  std::string getSignature(bytecode::FunctionType const &Type) const;

  /* __sable_instance_t * */ llvm::PointerType *getInstancePtrTy() const;

  /* __sable_memory_metadata_t   */
  llvm::StructType *getMemoryMetadataTy() const;
  /* __sable_table_metadata_t    */
  llvm::StructType *getTableMetadataTy() const;
  /* __sable_global_metadata_t   */
  llvm::StructType *getGlobalMetadataTy() const;
  /* __sable_function_metadata_t */
  llvm::StructType *getFunctionMetadataTy() const;

  /* __sable_memory_t *   */ llvm::PointerType *getMemoryPtrTy() const;
  /* __sable_table_t *    */ llvm::PointerType *getTablePtrTy() const;
  /* __sable_global_t *   */ llvm::PointerType *getGlobalPtrTy() const;
  /* __sable_function_t * */ llvm::PointerType *getFunctionPtrTy() const;

  llvm::GlobalVariable *getMemoryMetadata() const;
  llvm::GlobalVariable *getTableMetadata() const;
  llvm::GlobalVariable *getGlobalMetadata() const;
  llvm::GlobalVariable *getFunctionMetadata() const;
};

class FunctionTranslationTask {
  std::unique_ptr<TranslationContext> Context;

public:
  FunctionTranslationTask(
      EntityLayout &EntityLayout_, mir::Function const &Source_,
      llvm::Function &Target_);
  FunctionTranslationTask(FunctionTranslationTask const &) = delete;
  FunctionTranslationTask(FunctionTranslationTask &&) noexcept;
  FunctionTranslationTask &operator=(FunctionTranslationTask const &) = delete;
  FunctionTranslationTask &operator=(FunctionTranslationTask &&) noexcept;
  ~FunctionTranslationTask() noexcept;

  void perform();
};

class ModuleTranslationTask {
  std::unique_ptr<EntityLayout> Layout;
  mir::Module const *Source;
  llvm::Module *Target;
  TranslationOptions Options;

public:
  ModuleTranslationTask(
      mir::Module const &Source_, llvm::Module &Target,
      TranslationOptions Options_ = {});
  void perform();
};
} // namespace codegen::llvm_instance

#endif
