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
  llvm::StructType *getOpaqueTy(std::string_view Name);
  llvm::StructType *createNamedStructTy(std::string_view Name);
  llvm::StructType *getNamedStructTy(std::string_view Name);

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
  llvm::Type *convertType(bytecode::ValueType const &Type);
  llvm::FunctionType *convertType(bytecode::FunctionType const &Type);

  std::size_t getOffset(mir::ASTNode const &Node) const;

  llvm::Constant *operator[](mir::Data const &DataSegment) const;
  FunctionEntry const &operator[](mir::Function const &Function) const;
  ElementEntry const &operator[](mir::Element const &ElementSegment) const;

  llvm::Function *getBuiltin(std::string_view Name);

  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Global const &MGlobal);
  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Function const &MFunction);
  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Memory const &MMemory);
  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Table const &MTable);

  char getTypeChar(bytecode::ValueType const &Type);
  std::string getTypeString(bytecode::FunctionType const &Type);

  /* void          */ llvm::Type *getVoidTy();
  /* void *        */ llvm::PointerType *getVoidPtrTy();
  /* char const *  */ llvm::PointerType *getCStringPtrTy();
  llvm::Constant *getCStringPtr(std::string_view, std::string_view Name = "");
  /* std::int32_t  */ llvm::IntegerType *getI32Ty();
  llvm::Constant *getI32Constant(std::int32_t Value);
  /* std::int64_t  */ llvm::IntegerType *getI64Ty();
  llvm::Constant *getI64Constant(std::int64_t Value);
  /* float         */ llvm::Type *getF32Ty();
  llvm::Constant *getF32Constant(float Value);
  /* double        */ llvm::Type *getF64Ty();
  llvm::Constant *getF64Constant(double Value);
  /* std::intptr_t */ llvm::Type *getPtrIntTy();

  /* __sable_instance_t * */ llvm::PointerType *getInstancePtrTy();

  /* __sable_memory_metadata_t   */ llvm::StructType *getMemoryMetadataTy();
  /* __sable_table_metadata_t    */ llvm::StructType *getTableMetadataTy();
  /* __sable_global_metadata_t   */ llvm::StructType *getGlobalMetadataTy();
  /* __sable_function_metadata_t */ llvm::StructType *getFunctionMetadataTy();

  /* __sable_memory_t *   */ llvm::PointerType *getMemoryPtrTy();
  /* __sable_table_t *    */ llvm::PointerType *getTablePtrTy();
  /* __sable_global_t *   */ llvm::PointerType *getGlobalPtrTy();
  /* __sable_function_t * */ llvm::PointerType *getFunctionPtrTy();

  llvm::GlobalVariable *getMemoryMetadata();
  llvm::GlobalVariable *getTableMetadata();
  llvm::GlobalVariable *getGlobalMetadata();
  llvm::GlobalVariable *getFunctionMetadata();
};

class FunctionTranslationTask {
  class TranslationContext;
  class TranslationVisitor;
  std::unique_ptr<TranslationContext> Context;

public:
  FunctionTranslationTask(
      EntityLayout const &EntityLayout_, mir::Function const &Source_,
      llvm::Function &Target_);
  FunctionTranslationTask(FunctionTranslationTask const &) = delete;
  FunctionTranslationTask(FunctionTranslationTask &&) noexcept;
  FunctionTranslationTask &operator=(FunctionTranslationTask const &) = delete;
  FunctionTranslationTask &operator=(FunctionTranslationTask &&) noexcept;
  ~FunctionTranslationTask() noexcept;

  void perform();
};
} // namespace codegen::llvm_instance

#endif