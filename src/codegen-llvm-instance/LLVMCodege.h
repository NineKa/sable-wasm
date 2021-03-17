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
#include <unordered_map>

namespace codegen::llvm_instance {

class EntityLayout {
  mir::Module const &Source;
  llvm::Module &Target;

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

  llvm::DenseMap<mir::ASTNode const *, std::size_t> OffsetMap;
  void setupInstanceType();

  llvm::Type *convertType(bytecode::ValueType const &Type);
  llvm::FunctionType *convertType(bytecode::FunctionType const &Type);

  llvm::Type *getCStringPtrTy();
  llvm::Constant *getI32Constant(std::int32_t Value);
  llvm::Constant *getCString(std::string_view, std::string_view Name = "");
  llvm::Constant *getCStringPtr(std::string_view, std::string_view Name = "");
  char getTypeChar(bytecode::ValueType const &Type);
  std::string getTypeString(bytecode::FunctionType const &Type);

  llvm::Value *translateInitExpr(
      llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::ConstantExpr const &Expr);

  llvm::DenseMap<mir::DataSegment const *, llvm::GlobalVariable *> Data;
  void setupDataSegments();

  llvm::GlobalVariable *setupArrayGlobal(
      llvm::Type *ElementType, std::span<llvm::Constant *const> Elements);
  llvm::GlobalVariable *setupMetadataGlobals(
      std::string_view Prefix, std::uint32_t Size, std::uint32_t ImportSize,
      std::uint32_t ExportSize, llvm::GlobalVariable *Entities,
      llvm::GlobalVariable *Imports, llvm::GlobalVariable *Exports);

  void setupMemoryMetadata();
  void setupTableMetadata();
  void setupGlobalMetadata();
  void setupFunctionMetadata();

  struct FunctionEntry {
    llvm::Function *Definition;
    llvm::Constant *TypeString;
  };
  llvm::DenseMap<mir::Function const *, FunctionEntry> FunctionMap;
  FunctionEntry const &get(mir::Function const &Function) const;
  void setupFunctions();
  void setupImportForwarding(
      mir::Function const &MFunction, llvm::Function &LFunction);

  void setupInitializationFunction();

public:
  EntityLayout(mir::Module const &Source_, llvm::Module &Target_);
  std::size_t getOffset(mir::ASTNode const &Node) const;

  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Global const &MGlobal);
  llvm::Value *
  get(llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
      mir::Function const &MFunction);
};
} // namespace codegen::llvm_instance

#endif