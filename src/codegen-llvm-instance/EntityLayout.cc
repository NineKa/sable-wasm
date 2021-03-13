#include "LLVMCodege.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/LLVMContext.h>

namespace codegen::llvm_instance {
namespace {
namespace detail {
llvm::Type *getVoidPtrTy(llvm::LLVMContext &Context) {
  return llvm::Type::getInt8PtrTy(Context); // void *
}

llvm::Type *getFuncPtrTy(llvm::LLVMContext &Context) {
  return llvm::Type::getInt8PtrTy(Context); // void (*)()
}

void setupRuntimeSupport(llvm::Module &Module) {
  llvm::LLVMContext &Context = Module.getContext();

  auto *InstanceTy = llvm::StructType::create(Context, "__sable_instance_t");
  auto *InstancePtrTy = llvm::PointerType::getUnqual(InstanceTy);
  auto *GlobalTy = llvm::StructType::create(Context, "__sable_global_t");
  auto *GlobalPtrTy = llvm::PointerType::getUnqual(GlobalTy);
  auto *MemoryPtrTy = detail::getVoidPtrTy(Context);
  auto *TableTy = llvm::StructType::create(Context, "__sable_table_t");
  auto *TablePtrTy = llvm::PointerType::getUnqual(TableTy);

#define DEFINE_BUILTIN_FUNC(Name, RetType, ...)                                \
  llvm::Function::Create(                                                      \
      llvm::FunctionType::get(RetType, {__VA_ARGS__}, false),                  \
      llvm::GlobalValue::LinkageTypes::ExternalLinkage, #Name, Module)

  auto *InstanceGetterTy = llvm::PointerType::getUnqual(llvm::FunctionType::get(
      detail::getVoidPtrTy(Context),
      {InstancePtrTy, llvm::Type::getInt8PtrTy(Context)}, false));
  auto *TrapHandlerTy = llvm::PointerType::getUnqual(llvm::FunctionType::get(
      llvm::Type::getVoidTy(Context), {llvm::Type::getInt32Ty(Context)},
      false));
  DEFINE_BUILTIN_FUNC(
      __sable_instance_allocate,
      /* Ret */ InstancePtrTy,
      /* Arg GlobalGetter   */ InstanceGetterTy,
      /* Arg MemoryGetter   */ InstanceGetterTy,
      /* Arg TableGetter    */ InstanceGetterTy,
      /* Arg FunctionGetter */ InstanceGetterTy,
      /* Arg TrapHandler    */ TrapHandlerTy);

  DEFINE_BUILTIN_FUNC(
      __sable_intance_free,
      /* Ret                */ llvm::Type::getVoidTy(Context),
      /* Arg Instance       */ InstancePtrTy);

  DEFINE_BUILTIN_FUNC(
      __sable_global_allocate,
      /* Ret                */ GlobalPtrTy,
      /* Arg Type           */ llvm::Type::getInt8Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_global_free,
      /* Ret                */ llvm::Type::getVoidTy(Context),
      /* Arg Global         */ GlobalPtrTy);

  DEFINE_BUILTIN_FUNC(
      __sable_global_get,
      /* Ret                */ detail::getVoidPtrTy(Context),
      /* Arg Global         */ GlobalPtrTy);

  DEFINE_BUILTIN_FUNC(
      __sable_global_type,
      /* Ret                */ llvm::Type::getInt8Ty(Context),
      /* Arg Global         */ GlobalPtrTy);

  DEFINE_BUILTIN_FUNC(
      __sable_memory_allocate,
      /* Ret                */ MemoryPtrTy,
      /* Arg NumPage        */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_memory_allocate_with_bound,
      /* Ret                */ MemoryPtrTy,
      /* Arg NumPage        */ llvm::Type::getInt32Ty(Context),
      /* Arg MaxNumPage     */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_memory_free,
      /* Ret                */ llvm::Type::getVoidTy(Context),
      /* Arg Memory         */ MemoryPtrTy);

  DEFINE_BUILTIN_FUNC(
      __sable_memory_size,
      /* Ret                */ llvm::Type::getInt32Ty(Context),
      /* Arg Memory         */ MemoryPtrTy);

  DEFINE_BUILTIN_FUNC(
      __sable_memory_grow,
      /* Ret                */ llvm::Type::getInt32Ty(Context),
      /* Arg MemoryPtr      */ llvm::PointerType::getUnqual(MemoryPtrTy),
      /* Arg DeltaNumPage   */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_memory_guard,
      /* Ret                */ llvm::Type::getVoidTy(Context),
      /* Arg Instance       */ InstancePtrTy,
      /* Arg Memory         */ MemoryPtrTy,
      /* Arg Address        */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_table_allocate,
      /* Ret                */ TablePtrTy,
      /* Arg NumEntries     */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_table_allocate_with_bound,
      /* Ret                */ TablePtrTy,
      /* Arg NumEntries     */ llvm::Type::getInt32Ty(Context),
      /* Arg MaxNumEntries  */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_table_free,
      /* Ret                */ llvm::Type::getVoidTy(Context),
      /* Arg Table          */ TablePtrTy);

  DEFINE_BUILTIN_FUNC(
      __sable_table_size,
      /* Ret                */ llvm::Type::getInt32Ty(Context),
      /* Arg Table          */ TablePtrTy);

  DEFINE_BUILTIN_FUNC(
      __sable_table_guard,
      /* Ret                */ llvm::Type::getVoidTy(Context),
      /* Arg Instance       */ InstancePtrTy,
      /* Arg Table          */ TablePtrTy,
      /* Arg Index          */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_table_set,
      /* Ret                */ detail::getFuncPtrTy(Context),
      /* Arg Table          */ TablePtrTy,
      /* Arg FunctionPtr    */ detail::getFuncPtrTy(Context),
      /* Arg Index          */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_table_type,
      /* Ret                */ TablePtrTy,
      /* Arg Index          */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_table_get,
      /* Ret                */ detail::getFuncPtrTy(Context),
      /* Arg Table          */ TablePtrTy,
      /* Arg Index          */ llvm::Type::getInt32Ty(Context));

  DEFINE_BUILTIN_FUNC(
      __sable_strcmp,
      /* Ret                */ llvm::Type::getInt32Ty(Context),
      /* Arg LHS            */ llvm::Type::getInt8PtrTy(Context),
      /* Arg RHS            */ llvm::Type::getInt8PtrTy(Context));
#undef DEFINE_BUILTIN_FUNC
}
} // namespace detail
} // namespace

} // namespace codegen::llvm_instance
