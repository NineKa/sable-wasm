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
  llvm::Type *SableInstancePtrTy;

  /*
   * Instance Struct Layout:
   * __sable_memory_metadata_t *
   * __sable_table_metadata_t *
   * __sable_global_metadata_t *
   * __sable_function_metadata_t *
   * ...... Memory Instance Ptrs   ......
   * ...... Table Instance Ptrs    ......
   * ...... Global Instance Ptrs   ......
   * ...... Function Ptrs          ......
   */

  llvm::DenseMap<mir::ASTNode const *, std::size_t> OffsetMap;
  void setupOffsetMap();
  std::size_t getOffset(mir::ASTNode const &Node) const;

  llvm::Type *convertType(bytecode::ValueType const &Type);
  llvm::FunctionType *convertType(bytecode::FunctionType const &Type);

  llvm::Type *getCStringPtrTy();
  llvm::Constant *getI32Constant(std::int32_t Value);
  llvm::Constant *getCString(std::string_view, std::string_view Name = "");
  llvm::Constant *getCStringPtr(std::string_view, std::string_view Name = "");
  char getTypeChar(bytecode::ValueType const &Type);
  std::string getTypeString(bytecode::FunctionType const &Type);

  void setupFunctionMetadata();

  struct FunctionEntry {
    llvm::Function *Definition;
    llvm::Constant *TypeString;
  };
  llvm::DenseMap<mir::Function const *, FunctionEntry> FunctionMap;
  FunctionEntry const &get(mir::Function const &Function) const;
  void setupFunction();
  void setupImportForwarding(
      mir::Function const &MFunction, llvm::Function &LFunction);

public:
  EntityLayout(mir::Module const &Source_, llvm::Module &Target_);
};
} // namespace codegen::llvm_instance

#endif