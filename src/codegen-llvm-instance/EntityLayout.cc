#include "LLVMCodege.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/transform.hpp>

namespace codegen::llvm_instance {
void EntityLayout::setupOffsetMap() {
  for (auto const &Memory : Source.getMemories())
    OffsetMap.insert(std::make_pair(std::addressof(Memory), OffsetMap.size()));
  for (auto const &Table : Source.getTables())
    OffsetMap.insert(std::make_pair(std::addressof(Table), OffsetMap.size()));
  for (auto const &Global : Source.getGlobals())
    OffsetMap.insert(std::make_pair(std::addressof(Global), OffsetMap.size()));
  for (auto const &Function : Source.getFunctions())
    OffsetMap.insert(
        std::make_pair(std::addressof(Function), OffsetMap.size()));
}

std::size_t EntityLayout::getOffset(mir::ASTNode const &Node) const {
  auto SearchIter = OffsetMap.find(std::addressof(Node));
  assert(SearchIter != OffsetMap.end());
  return std::get<1>(*SearchIter) + 4;
}

llvm::Type *EntityLayout::convertType(bytecode::ValueType const &Type) {
  auto &Context = Target.getContext();
  using VKind = bytecode::ValueTypeKind;
  switch (Type.getKind()) {
  case VKind::I32: return llvm::Type::getInt32Ty(Context);
  case VKind::I64: return llvm::Type::getInt64Ty(Context);
  case VKind::F32: return llvm::Type::getFloatTy(Context);
  case VKind::F64: return llvm::Type::getDoubleTy(Context);
  default: SABLE_UNREACHABLE();
  }
}

llvm::FunctionType *
EntityLayout::convertType(bytecode::FunctionType const &Type) {
  auto &Context = Target.getContext();
  std::vector<llvm::Type *> ParamTypes;
  ParamTypes.reserve(Type.getNumParameter() + 1);
  ParamTypes.push_back(SableInstancePtrTy);
  for (auto const &ValueType : Type.getParamTypes())
    ParamTypes.push_back(convertType(ValueType));
  if (Type.isVoidResult()) {
    auto *ResultType = llvm::Type::getVoidTy(Context);
    return llvm::FunctionType::get(ResultType, ParamTypes, false);
  }
  if (Type.isSingleValueResult()) {
    auto *ResultType = convertType(Type.getResultTypes()[0]);
    return llvm::FunctionType::get(ResultType, ParamTypes, false);
  }
  if (Type.isMultiValueResult()) {
    // clang-format off
    auto ResultTypes = Type.getResultTypes() 
        | ranges::views::transform([&](bytecode::ValueType const &ValueType) {
            return convertType(ValueType);
          }) 
        | ranges::to<std::vector<llvm::Type *>>();
    // clang-format on
    auto *ResultType = llvm::StructType::get(Context, ResultTypes);
    return llvm::FunctionType::get(ResultType, ParamTypes, false);
  }
  SABLE_UNREACHABLE();
}

llvm::Type *EntityLayout::getCStringPtrTy() {
  return llvm::Type::getInt8PtrTy(Target.getContext());
}

llvm::Constant *EntityLayout::getI32Constant(std::int32_t Value) {
  auto &Context = Target.getContext();
  auto *ContentConstant =
      llvm::ConstantInt::get(llvm::Type::getInt32Ty(Context), Value);
  return ContentConstant;
}

llvm::Constant *
EntityLayout::getCString(std::string_view Content, std::string_view Name) {
  auto &Context = Target.getContext();
  auto *ContentConstant = llvm::ConstantDataArray::getString(Context, Content);
  auto *CString = new llvm::GlobalVariable(
      /* Parent      */ Target,
      /* Type        */ ContentConstant->getType(),
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::PrivateLinkage,
      /* Initializer */ ContentConstant);
  CString->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
  CString->setAlignment(llvm::Align(1));
  CString->setName(llvm::StringRef(Name));
  return CString;
}

llvm::Constant *
EntityLayout::getCStringPtr(std::string_view Content, std::string_view Name) {
  auto *CString = getCString(Content, Name);
  auto *Zero = getI32Constant(0);
  std::array<llvm::Constant *, 2> Indices{Zero, Zero};
  return llvm::ConstantExpr::getInBoundsGetElementPtr(
      llvm::dyn_cast<llvm::GlobalVariable>(CString)->getValueType(), CString,
      Indices);
}

char EntityLayout::getTypeChar(bytecode::ValueType const &Type) {
  switch (Type.getKind()) {
  case bytecode::ValueTypeKind::I32: return 'I';
  case bytecode::ValueTypeKind::I64: return 'J';
  case bytecode::ValueTypeKind::F32: return 'F';
  case bytecode::ValueTypeKind::F64: return 'D';
  default: SABLE_UNREACHABLE();
  }
}

std::string EntityLayout::getTypeString(bytecode::FunctionType const &Type) {
  std::string Result;
  Result.reserve(Type.getNumParameter() + Type.getNumResult() + 1);
  for (auto const &ValueType : Type.getParamTypes())
    Result.push_back(getTypeChar(ValueType));
  Result.push_back(':');
  for (auto const &ValueType : Type.getResultTypes())
    Result.push_back(getTypeChar(ValueType));
  return Result;
}

void EntityLayout::setupFunctionMetadata() {
  auto &Context = Target.getContext();

  auto *EntityTy = getCStringPtrTy();
  auto *ImportTy = llvm::StructType::get(
      Context,
      {llvm::Type::getInt32Ty(Context), getCStringPtrTy(), getCStringPtrTy()});
  auto *ExportTy = llvm::StructType::get(
      Context, {llvm::Type::getInt32Ty(Context), getCStringPtrTy()});
  std::vector<llvm::Constant *> Entities;
  std::vector<llvm::Constant *> Imports;
  std::vector<llvm::Constant *> Exports;

  for (auto const &Function : Source.getFunctions())
    Entities.push_back(get(Function).TypeString);
  for (auto const &[Index, Function] :
       ranges::views::enumerate(Source.getFunctions())) {
    if (!Function.isImported()) continue;
    auto *ModuleName = getCStringPtr(Function.getImportModuleName());
    auto *EntityName = getCStringPtr(Function.getImportEntityName());
    auto *ImportConstant = llvm::ConstantStruct::get(
        ImportTy, {getI32Constant(Index), ModuleName, EntityName});
    Imports.push_back(ImportConstant);
  }
  for (auto const &[Index, Function] :
       ranges::views::enumerate(Source.getFunctions())) {
    if (!Function.isExported()) continue;
    auto *EntityName = getCStringPtr(Function.getExportName());
    auto *ExportConstant = llvm::ConstantStruct::get(
        ExportTy, {getI32Constant(Index), EntityName});
    Exports.push_back(ExportConstant);
  }

  auto *EntitiesConstant = llvm::ConstantArray::get(
      llvm::ArrayType::get(EntityTy, Entities.size()), Entities);
  auto *EntitiesGlobal = new llvm::GlobalVariable(
      /* Parent      */ Target,
      /* Type        */ EntitiesConstant->getType(),
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::PrivateLinkage,
      /* Initializer */ EntitiesConstant,
      /* Name        */ "__sable_function_metadata.entities");
  auto *ImportsConstant = llvm::ConstantArray::get(
      llvm::ArrayType::get(ImportTy, Imports.size()), Imports);
  auto *ImportsGlobal = new llvm::GlobalVariable(
      /* Parent      */ Target,
      /* Type        */ ImportsConstant->getType(),
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::PrivateLinkage,
      /* Initializer */ ImportsConstant,
      /* Name        */ "__sable_function_metadata.imports");
  auto *ExportsConstant = llvm::ConstantArray::get(
      llvm::ArrayType::get(ExportTy, Exports.size()), Exports);
  auto *ExportsGlobal = new llvm::GlobalVariable(
      /* Parent      */ Target,
      /* Type        */ ExportsConstant->getType(),
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::PrivateLinkage,
      /* Initializer */ ExportsConstant,
      /* Name        */ "__sable_function_metadata.exports");
  auto *MetadataTy = llvm::StructType::get(
      Context,
      {llvm::Type::getInt32Ty(Context), llvm::Type::getInt32Ty(Context),
       llvm::Type::getInt32Ty(Context), EntitiesGlobal->getType(),
       ImportsGlobal->getType(), ExportsGlobal->getType()});
  auto *MetadataConstant = llvm::ConstantStruct::get(
      MetadataTy,
      {getI32Constant(Entities.size()), getI32Constant(Imports.size()),
       getI32Constant(Exports.size()), EntitiesGlobal, ImportsGlobal,
       ExportsGlobal});
  new llvm::GlobalVariable(
      /* Parent      */ Target,
      /* Type        */ MetadataTy,
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::ExternalLinkage,
      /* Initializer */ MetadataConstant,
      /* Name        */ "__sable_function_metadata");
}

EntityLayout::FunctionEntry const &
EntityLayout::get(mir::Function const &Function) const {
  auto SearchIter = FunctionMap.find(std::addressof(Function));
  assert(SearchIter != FunctionMap.end());
  return std::get<1>(*SearchIter);
}

void EntityLayout::setupFunction() {
  for (auto const &Function : Source.getFunctions()) {
    auto *TypeString = getCStringPtr(
        getTypeString(Function.getType()),
        Function.hasName() ? fmt::format("typestr.{}", Function.getName())
                           : "typestr");
    auto *Definition = llvm::Function::Create(
        /* Type    */ convertType(Function.getType()),
        /* Linkage */ llvm::GlobalVariable::PrivateLinkage,
        /* Name    */ llvm::StringRef(Function.getName()),
        /* Parent  */ Target);
    FunctionMap.insert(std::make_pair(
        std::addressof(Function),
        FunctionEntry{.Definition = Definition, .TypeString = TypeString}));
    if (Function.isImported()) setupImportForwarding(Function, *Definition);
  }
}

void EntityLayout::setupImportForwarding(
    mir::Function const &MFunction, llvm::Function &LFunction) {
  auto Offset = getOffset(MFunction);
  auto *EntryBasicBlock = llvm::BasicBlock::Create(
      /* Context */ Target.getContext(),
      /* Name    */ "entry",
      /* Parent  */ std::addressof(LFunction));
  llvm::IRBuilder<> Builder(EntryBasicBlock);
  // clang-format off
  auto Arguments = ranges::subrange(LFunction.arg_begin(), LFunction.arg_end())
    | ranges::views::transform([](llvm::Argument &Argument) {
        return std::addressof(Argument);
      })
    | ranges::to<std::vector<llvm::Value *>>();
  // clang-format on
}

EntityLayout::EntityLayout(mir::Module const &Source_, llvm::Module &Target_)
    : Source(Source_), Target(Target_), SableInstancePtrTy() {
  auto *SableInstanceTy = llvm::StructType::get(Target.getContext());
  SableInstanceTy->setName("__sable_instance_t");
  SableInstancePtrTy = llvm::PointerType::getUnqual(SableInstanceTy);

  setupOffsetMap();
  setupFunction();
  setupFunctionMetadata();
}
} // namespace codegen::llvm_instance
