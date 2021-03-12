#include "LLVMCodege.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/LLVMContext.h>

namespace codegen::llvm_instance {
void setupRuntimeSupport(
    bytecode::ModuleView const &BModule, llvm::Module &Module) {

  llvm::LLVMContext &Context = Module.getContext();

  auto NumImportedEntities =
      BModule.getNumImportedTables() + BModule.getNumImportedMemories() +
      BModule.getNumImportedGlobals() + BModule.getNumImportedFunctions();
  auto *InstanceTy = llvm::ArrayType::get(
      llvm::Type::getInt8PtrTy(Context), 5 + NumImportedEntities);
  auto *InstancePtrTy = llvm::PointerType::getUnqual(InstanceTy);

  auto *InstanceGetterTy = llvm::PointerType::getUnqual(llvm::FunctionType::get(
      llvm::Type::getVoidTy(Context),
      {InstancePtrTy, llvm::Type::getInt8PtrTy(Context)}, false));
  auto *TrapHandlerTy = llvm::PointerType::getUnqual(llvm::FunctionType::get(
      llvm::Type::getVoidTy(Context), {llvm::Type::getInt32Ty(Context)},
      false));

  auto *InstanceAllocateFnTy = llvm::FunctionType::get(
      InstancePtrTy,
      {InstanceGetterTy, InstanceGetterTy, InstanceGetterTy, InstanceGetterTy,
       TrapHandlerTy},
      false);
  llvm::Function::Create(
      InstanceAllocateFnTy, llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      "__sable_instance_allocate", Module);

  auto *InstanceFreeFnTy = llvm::FunctionType::get(
      llvm::Type::getVoidTy(Context), {InstancePtrTy}, false);
  llvm::Function::Create(
      InstanceFreeFnTy, llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      "__sable_instance_free", Module);

  auto *GlobalTy = llvm::StructType::create(Context);
  GlobalTy->setName("__sable_global_t");
  auto *GlobalPtrTy = llvm::PointerType::getUnqual(GlobalTy);

  auto *GlobalAllocateFnTy = llvm::FunctionType::get(
      GlobalPtrTy, {llvm::Type::getInt8Ty(Context)}, false);
  llvm::Function::Create(
      GlobalAllocateFnTy, llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      "__sable_global_allocate", Module);
}

} // namespace codegen::llvm_instance
