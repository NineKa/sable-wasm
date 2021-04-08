#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_LLVM_CODEGEN
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_LLVM_CODEGEN

#include "../bytecode/Module.h"
#include "../mir/Module.h"

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
      llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
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

  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Global const &Global) const;
  llvm::Value *getContextPtr(
      llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Function const &Function) const;
  llvm::Value *getFunctionPtr(
      llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Function const &Function) const;
  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Memory const &Memory) const;
  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Table const &Table) const;

  char getSignature(bytecode::ValueType const &Type) const;
  char getSignature(bytecode::GlobalType const &Type) const;
  std::string getSignature(bytecode::FunctionType const &Type) const;

  /* void          */ llvm::Type *getVoidTy() const;
  /* void *        */ llvm::PointerType *getVoidPtrTy() const;
  /* char const *  */ llvm::PointerType *getCStringPtrTy() const;
  llvm::Constant *
  getCStringPtr(std::string_view, std::string_view Name = "") const;
  /* std::int32_t  */ llvm::IntegerType *getI32Ty() const;
  llvm::ConstantInt *getI32Constant(std::int32_t Value) const;
  /* std::int64_t  */ llvm::IntegerType *getI64Ty() const;
  llvm::ConstantInt *getI64Constant(std::int64_t Value) const;
  /* float         */ llvm::Type *getF32Ty() const;
  llvm::ConstantFP *getF32Constant(float Value) const;
  /* double        */ llvm::Type *getF64Ty() const;
  llvm::ConstantFP *getF64Constant(double Value) const;
  /* std::intptr_t */ llvm::Type *getPtrIntTy() const;

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
  class TranslationContext;
  class TranslationVisitor;
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
