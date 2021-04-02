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

class EntityLayout {
public:
  class FunctionEntry {
    llvm::Function *Definition;
    llvm::Constant *TypeString;

  public:
    FunctionEntry(llvm::Function *Definition_, llvm::Constant *TypeString_)
        : Definition(Definition_), TypeString(TypeString_) {}
    llvm::Function *definition() const { return Definition; }
    llvm::Constant *typeString() const { return TypeString; }
  };

  class ElementEntry {
    llvm::Constant *Pointers;
    llvm::Constant *TypeStrings;

  public:
    ElementEntry(llvm::Constant *Pointers_, llvm::Constant *TypeStrings_)
        : Pointers(Pointers_), TypeStrings(TypeStrings_) {}
    llvm::Constant *pointers() const { return Pointers; }
    llvm::Constant *typeStrings() const { return TypeStrings; }
  };

private:
  mir::Module const &Source;
  llvm::Module &Target;
  llvm::DenseMap<mir::ASTNode const *, std::size_t> OffsetMap;
  llvm::DenseMap<mir::Data const *, llvm::Constant *> DataMap;
  llvm::DenseMap<mir::Element const *, ElementEntry> ElementMap;
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
   * ...... Memory Instance Pointers   ......
   * ...... Table Instance Pointers    ......
   * ...... Global Instance Pointers   ......
   * ...... Function Pointers          ......
   */

  void setupInstanceType();

  llvm::Value *translateInitExpr(
      llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::InitializerExpr const &Expr);

  void setupDataSegments();
  void setupElementSegments();

  llvm::GlobalVariable *setupArrayGlobal(
      llvm::Type *ElementType, std::span<llvm::Constant *const> Elements);
  llvm::GlobalVariable *setupMetadata(
      std::string_view Prefix, std::uint32_t Size, std::uint32_t ImportSize,
      std::uint32_t ExportSize, llvm::GlobalVariable *Entities,
      llvm::GlobalVariable *Imports, llvm::GlobalVariable *Exports);

  void setupMemoryMetadata();
  void setupTableMetadata();
  void setupGlobalMetadata();
  void setupFunctionMetadata();
  void setupFunctions();
  void setupInitialization();
  void setupBuiltins();

public:
  EntityLayout(mir::Module const &Source_, llvm::Module &Target_);
  llvm::Type *convertType(bytecode::ValueType const &Type) const;
  llvm::FunctionType *convertType(bytecode::FunctionType const &Type) const;

  std::size_t getOffset(mir::ASTNode const &Node) const;

  llvm::Constant *operator[](mir::Data const &DataSegment) const;
  FunctionEntry const &operator[](mir::Function const &Function) const;
  ElementEntry const &operator[](mir::Element const &ElementSegment) const;

  /* List of Builtins (implement by the runtime library
   * __sable_memory_guard
   * __sable_table_guard
   * __sable_table_set    (* no boundary check is required *)
   * __sable_table_get    (* no boundary check is required *)
   * error handling:
   * __sable_unreachable
   * __sable_conversion_overflow
   * __sable_zero_divisor
   */
  llvm::Function *getBuiltin(std::string_view Name) const;

  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Global const &MGlobal) const;
  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Function const &MFunction) const;
  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Memory const &MMemory) const;
  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Table const &MTable) const;

  char getTypeChar(bytecode::ValueType const &Type) const;
  std::string getTypeString(bytecode::FunctionType const &Type) const;

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

public:
  ModuleTranslationTask(mir::Module const &Source_, llvm::Module &Target);
  void perform();
};
} // namespace codegen::llvm_instance

#endif
