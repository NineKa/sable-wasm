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

#include <fmt/format.h>

#include <limits>

namespace codegen::llvm_instance {

llvm::StructType *EntityLayout::declareOpaqueTy(std::string_view Name) {
  return llvm::StructType::create(Target.getContext(), Name);
}

llvm::StructType *EntityLayout::createNamedStructTy(std::string_view Name) {
  return llvm::StructType::create(Target.getContext(), Name);
}

llvm::StructType *EntityLayout::getNamedStructTy(std::string_view Name) {
  auto *Type = Target.getTypeByName(Name);
  assert((Type != nullptr) && (llvm::isa<llvm::StructType>(Type)));
  return llvm::dyn_cast<llvm::StructType>(Type);
}

llvm::StructType *EntityLayout::getOpaqueTy(std::string_view Name) {
  auto *Type = Target.getTypeByName(Name);
  assert((Type != nullptr) && (llvm::isa<llvm::StructType>(Type)));
  return llvm::dyn_cast<llvm::StructType>(Type);
}

void EntityLayout::setupInstanceType() {
  auto *InstanceTy = createNamedStructTy("__sable_instance_t");
  std::vector<llvm::Type *> InstanceFields;

  auto *MemoryMetadataTy = createNamedStructTy("__sable_memory_metadata_t");
  auto *TableMetadataTy = createNamedStructTy("__sable_table_metadata_t");
  auto *GlobalMetadataTy = createNamedStructTy("__sable_global_metadata_t");
  auto *FunctionMetadataTy = createNamedStructTy("__sable_function_metadata_t");
  InstanceFields.push_back(llvm::PointerType::getUnqual(MemoryMetadataTy));
  InstanceFields.push_back(llvm::PointerType::getUnqual(TableMetadataTy));
  InstanceFields.push_back(llvm::PointerType::getUnqual(GlobalMetadataTy));
  InstanceFields.push_back(llvm::PointerType::getUnqual(FunctionMetadataTy));

  auto *MemoryOpaqueTy = declareOpaqueTy("__sable_memory_t");
  auto *MemoryOpaquePtrTy = llvm::PointerType::getUnqual(MemoryOpaqueTy);
  for (auto const &Memory : Source.getMemories()) {
    OffsetMap.insert(std::make_pair(std::addressof(Memory), OffsetMap.size()));
    InstanceFields.push_back(MemoryOpaquePtrTy);
  }

  auto *TableOpaqueTy = declareOpaqueTy("__sable_table_t");
  auto *TableOpaquePtrTy = llvm::PointerType::getUnqual(TableOpaqueTy);
  for (auto const &Table : Source.getTables()) {
    OffsetMap.insert(std::make_pair(std::addressof(Table), OffsetMap.size()));
    InstanceFields.push_back(TableOpaquePtrTy);
  }

  auto *GlobalOpaqueTy = declareOpaqueTy("__sable_global_t");
  auto *GlobalOpaquePtrTy = llvm::PointerType::getUnqual(GlobalOpaqueTy);
  for (auto const &Global : Source.getGlobals()) {
    OffsetMap.insert(std::make_pair(std::addressof(Global), OffsetMap.size()));
    InstanceFields.push_back(GlobalOpaquePtrTy);
  }

  auto *FunctionOpaqueTy = declareOpaqueTy("__sable_function_t");
  auto *FunctionOpaquePtrTy = llvm::PointerType::getUnqual(FunctionOpaqueTy);
  for (auto const &Function : Source.getFunctions()) {
    auto *FunctionAddress = std::addressof(Function);
    OffsetMap.insert(std::make_pair(FunctionAddress, OffsetMap.size()));
    InstanceFields.push_back(FunctionOpaquePtrTy);
  }

  InstanceTy->setBody(InstanceFields);
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
  auto *InstancePtrTy =
      llvm::PointerType::getUnqual(getNamedStructTy("__sable_instance_t"));
  std::vector<llvm::Type *> ParamTypes;
  ParamTypes.reserve(Type.getNumParameter() + 1);
  ParamTypes.push_back(InstancePtrTy);
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
  auto *CastedPtr = llvm::dyn_cast<llvm::GlobalVariable>(CString);
  return llvm::ConstantExpr::getInBoundsGetElementPtr(
      CastedPtr->getValueType(), CString, Indices);
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

llvm::Value *EntityLayout::translateInitExpr(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::ConstantExpr const &Expr) {
  using VKind = bytecode::ValueTypeKind;
  switch (Expr.getConstantExprKind()) {
  case mir::ConstantExprKind::Constant: {
    auto *CastedPtr =
        mir::dyn_cast<mir::constant_expr::Constant>(std::addressof(Expr));

    switch (CastedPtr->getValueType().getKind()) {
    case VKind::I32: return Builder.getInt32(CastedPtr->asI32());
    case VKind::I64: return Builder.getInt64(CastedPtr->asI64());
    case VKind::F32:
      return llvm::ConstantFP::get(Builder.getFloatTy(), CastedPtr->asF32());
    case VKind::F64:
      return llvm::ConstantFP::get(Builder.getDoubleTy(), CastedPtr->asF64());
    default: SABLE_UNREACHABLE();
    }
  }
  case mir::ConstantExprKind::GlobalGet: {
    auto *CastedPtr =
        mir::dyn_cast<mir::constant_expr::GlobalGet>(std::addressof(Expr));
    auto *TargetGlobal = CastedPtr->getGlobalValue();
    auto *GlobalPtr = get(Builder, InstancePtr, *TargetGlobal);
    auto *GlobalValue = Builder.CreateLoad(GlobalPtr);
    return GlobalValue;
  }
  default: SABLE_UNREACHABLE();
  }
}

void EntityLayout::setupDataSegments() {
  auto &Context = Target.getContext();
  for (auto const &DataSegment : Source.getData()) {
    auto ByteView = DataSegment.getContent();
    llvm::StringRef CharView(
        reinterpret_cast<char const *>(ByteView.data()), // NOLINT
        ByteView.size());
    auto *DataConstant =
        llvm::ConstantDataArray::getString(Context, CharView, false);
    auto *DataGlobal = new llvm::GlobalVariable(
        /* Parent      */ Target,
        /* Type        */ DataConstant->getType(),
        /* IsConstant  */ true,
        /* Linkage     */ llvm::GlobalVariable::LinkageTypes::PrivateLinkage,
        /* Initializer */ DataConstant,
        /* Name        */ "data");
    DataGlobal->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    DataGlobal->setAlignment(llvm::Align(1));
    Data.insert(std::make_pair(std::addressof(DataSegment), DataGlobal));
  }
}

llvm::GlobalVariable *EntityLayout::setupArrayGlobal(
    llvm::Type *ElementType, std::span<llvm::Constant *const> Elements) {
  auto *ElementsConstant = llvm::ConstantArray::get(
      llvm::ArrayType::get(ElementType, Elements.size()),
      llvm::ArrayRef(Elements.data(), Elements.size()));
  auto *ElementsGlobal = new llvm::GlobalVariable(
      /* Parent      */ Target,
      /* Type        */ ElementsConstant->getType(),
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::LinkageTypes::PrivateLinkage,
      /* Initializer */ ElementsConstant);
  return ElementsGlobal;
}

llvm::GlobalVariable *EntityLayout::setupMetadataGlobals(
    std::string_view Prefix, std::uint32_t Size, std::uint32_t ImportSize,
    std::uint32_t ExportSize, llvm::GlobalVariable *Entities,
    llvm::GlobalVariable *Imports, llvm::GlobalVariable *Exports) {
  auto &Context = Target.getContext();

  Entities->setName(fmt::format("{}.entities", Prefix));
  Entities->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
  Imports->setName(fmt::format("{}.imports", Prefix));
  Imports->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
  Exports->setName(fmt::format("{}.exports", Prefix));
  Exports->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);

  auto TypeName = fmt::format("{}_t", Prefix);
  auto *MetadataTy = getNamedStructTy(TypeName);
  MetadataTy->setBody(
      {/* Size       */ llvm::Type::getInt32Ty(Context),
       /* ImportSize */ llvm::Type::getInt32Ty(Context),
       /* ExportSize */ llvm::Type::getInt32Ty(Context),
       /* Entities   */ Entities->getType(),
       /* Imports    */ Imports->getType(),
       /* Exports    */ Exports->getType()});

  auto *MetadataConstant = llvm::ConstantStruct::get(
      MetadataTy, {/* Size       */ getI32Constant(Size),
                   /* ImportSize */ getI32Constant(ImportSize),
                   /* ExportSize */ getI32Constant(ExportSize),
                   /* Entities   */ Entities,
                   /* Imports    */ Imports,
                   /* Exports    */ Exports});
  return new llvm::GlobalVariable(
      /* Parent      */ Target,
      /* Type        */ MetadataTy,
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::ExternalLinkage,
      /* Initializer */ MetadataConstant,
      /* Name        */ llvm::StringRef(Prefix));
}

void EntityLayout::setupMemoryMetadata() {
  auto &Context = Target.getContext();

  auto *EntityTy = llvm::StructType::get(
      Context,
      {llvm::Type::getInt32Ty(Context), llvm::Type::getInt32Ty(Context)});
  auto *ImportTy = llvm::StructType::get(
      Context,
      {llvm::Type::getInt32Ty(Context), getCStringPtrTy(), getCStringPtrTy()});
  auto *ExportTy = llvm::StructType::get(
      Context, {llvm::Type::getInt32Ty(Context), getCStringPtrTy()});
  std::vector<llvm::Constant *> Entities;
  std::vector<llvm::Constant *> Imports;
  std::vector<llvm::Constant *> Exports;

  for (auto const &Memory : Source.getMemories()) {
    auto Min = Memory.getType().getMin();
    auto Max = Memory.getType().hasMax()
                   ? Memory.getType().getMax()
                   : std::numeric_limits<std::uint32_t>::max();
    auto *EntityConstant = llvm::ConstantStruct::get(
        EntityTy, {getI32Constant(Min), getI32Constant(Max)});
    Entities.push_back(EntityConstant);
  }
  for (auto const &[Index, Memory] :
       ranges::views::enumerate(Source.getMemories())) {
    if (!Memory.isImported()) continue;
    auto *ModuleName = getCStringPtr(Memory.getImportModuleName());
    auto *EntityName = getCStringPtr(Memory.getImportEntityName());
    auto *ImportConstant = llvm::ConstantStruct::get(
        ImportTy, {getI32Constant(Index), ModuleName, EntityName});
    Imports.push_back(ImportConstant);
  }
  for (auto const &[Index, Memory] :
       ranges::views::enumerate(Source.getMemories())) {
    if (!Memory.isExported()) continue;
    auto *EntityName = getCStringPtr(Memory.getExportName());
    auto *ExportConstant = llvm::ConstantStruct::get(
        ExportTy, {getI32Constant(Index), EntityName});
    Exports.push_back(ExportConstant);
  }

  setupMetadataGlobals(
      "__sable_memory_metadata",
      /* Sizes    */ Entities.size(), Imports.size(), Exports.size(),
      /* Entities */ setupArrayGlobal(EntityTy, Entities),
      /* Imports  */ setupArrayGlobal(ImportTy, Imports),
      /* Exports  */ setupArrayGlobal(ExportTy, Exports));
}

void EntityLayout::setupTableMetadata() {
  auto &Context = Target.getContext();

  auto *EntityTy = llvm::StructType::get(
      Context,
      {llvm::Type::getInt32Ty(Context), llvm::Type::getInt32Ty(Context)});
  auto *ImportTy = llvm::StructType::get(
      Context,
      {llvm::Type::getInt32Ty(Context), getCStringPtrTy(), getCStringPtrTy()});
  auto *ExportTy = llvm::StructType::get(
      Context, {llvm::Type::getInt32Ty(Context), getCStringPtrTy()});
  std::vector<llvm::Constant *> Entities;
  std::vector<llvm::Constant *> Imports;
  std::vector<llvm::Constant *> Exports;

  for (auto const &Table : Source.getTables()) {
    auto Min = Table.getType().getMin();
    auto Max = Table.getType().hasMax()
                   ? Table.getType().getMin()
                   : std::numeric_limits<std::uint32_t>::max();
    auto *EntityConstant = llvm::ConstantStruct::get(
        EntityTy, {getI32Constant(Min), getI32Constant(Max)});
    Entities.push_back(EntityConstant);
  }
  for (auto const &[Index, Table] :
       ranges::views::enumerate(Source.getTables())) {
    if (!Table.isImported()) continue;
    auto *ModuleName = getCStringPtr(Table.getImportModuleName());
    auto *EntityName = getCStringPtr(Table.getImportEntityName());
    auto *ImportConstant = llvm::ConstantStruct::get(
        ImportTy, {getI32Constant(Index), ModuleName, EntityName});
    Imports.push_back(ImportConstant);
  }
  for (auto const &[Index, Table] :
       ranges::views::enumerate(Source.getTables())) {
    if (!Table.isExported()) continue;
    auto *EntityName = getCStringPtr(Table.getExportName());
    auto *ExportConstant = llvm::ConstantStruct::get(
        ExportTy, {getI32Constant(Index), EntityName});
    Exports.push_back(ExportConstant);
  }

  setupMetadataGlobals(
      "__sable_table_metadata",
      /* Sizes    */ Entities.size(), Imports.size(), Exports.size(),
      /* Entities */ setupArrayGlobal(EntityTy, Entities),
      /* Imports  */ setupArrayGlobal(ImportTy, Imports),
      /* Exports  */ setupArrayGlobal(ExportTy, Exports));
}

void EntityLayout::setupGlobalMetadata() {
  auto &Context = Target.getContext();

  auto *ImportTy = llvm::StructType::get(
      Context,
      {llvm::Type::getInt32Ty(Context), getCStringPtrTy(), getCStringPtrTy()});
  auto *ExportTy = llvm::StructType::get(
      Context, {llvm::Type::getInt32Ty(Context), getCStringPtrTy()});
  std::string EntitiesString;
  std::vector<llvm::Constant *> Imports;
  std::vector<llvm::Constant *> Exports;

  for (auto const &Global : Source.getGlobals()) {
    char TypeChar = getTypeChar(Global.getType().getType());
    switch (Global.getType().getMutability()) {
    case bytecode::MutabilityKind::Var:
      TypeChar = static_cast<char>(tolower(TypeChar));
      break;
    case bytecode::MutabilityKind::Const:
      TypeChar = static_cast<char>(toupper(TypeChar));
      break;
    default: SABLE_UNREACHABLE();
    }
    EntitiesString.push_back(TypeChar);
  }
  for (auto const &[Index, Global] :
       ranges::views::enumerate(Source.getGlobals())) {
    if (!Global.isImported()) continue;
    auto *ModuleName = getCStringPtr(Global.getImportModuleName());
    auto *EntityName = getCStringPtr(Global.getImportEntityName());
    auto *ImportConstant = llvm::ConstantStruct::get(
        ImportTy, {getI32Constant(Index), ModuleName, EntityName});
    Imports.push_back(ImportConstant);
  }
  for (auto const &[Index, Global] :
       ranges::views::enumerate(Source.getGlobals())) {
    if (!Global.isExported()) continue;
    auto *EntityName = getCStringPtr(Global.getExportName());
    auto *ExportConstant = llvm::ConstantStruct::get(
        ExportTy, {getI32Constant(Index), EntityName});
    Exports.push_back(ExportConstant);
  }

  llvm::Constant *EntitiesConstant =
      llvm::ConstantDataArray::getString(Context, EntitiesString, false);
  auto *Entities = new llvm::GlobalVariable(
      /* Parent      */ Target,
      /* Type        */ EntitiesConstant->getType(),
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::LinkageTypes::PrivateLinkage,
      /* Initializer */ EntitiesConstant);

  setupMetadataGlobals(
      "__sable_global_metadata",
      /* Sizes    */ EntitiesString.size(), Imports.size(), Exports.size(),
      /* Entities */ Entities,
      /* Imports  */ setupArrayGlobal(ImportTy, Imports),
      /* Exports  */ setupArrayGlobal(ExportTy, Exports));
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

  setupMetadataGlobals(
      "__sable_function_metadata",
      /* Sizes    */ Entities.size(), Imports.size(), Exports.size(),
      /* Entities */ setupArrayGlobal(EntityTy, Entities),
      /* Imports  */ setupArrayGlobal(ImportTy, Imports),
      /* Exports  */ setupArrayGlobal(ExportTy, Exports));
}

EntityLayout::FunctionEntry const &
EntityLayout::get(mir::Function const &Function) const {
  auto SearchIter = FunctionMap.find(std::addressof(Function));
  assert(SearchIter != FunctionMap.end());
  return std::get<1>(*SearchIter);
}

void EntityLayout::setupFunctions() {
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
    if (Function.isImported()) {
      setupImportForwarding(Function, *Definition);
    } else {
      // TODO: do proper translation!
      auto *EntryBasicBlock =
          llvm::BasicBlock::Create(Target.getContext(), "entry", Definition);
      llvm::IRBuilder<> Builder(EntryBasicBlock);
      Builder.CreateUnreachable();
    }
  }
}

void EntityLayout::setupImportForwarding(
    mir::Function const &MFunction, llvm::Function &LFunction) {
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
  auto Offset = getOffset(MFunction);
  auto *CalleeTy = convertType(MFunction.getType());
  auto *CalleePtrTy = llvm::PointerType::getUnqual(CalleeTy);
  llvm::Value *CalleePtr = Builder.CreateStructGEP(Arguments[0], Offset);
  CalleePtr = Builder.CreateLoad(CalleePtr);
  CalleePtr = Builder.CreateBitCast(CalleePtr, CalleePtrTy);
  llvm::FunctionCallee Callee(CalleeTy, CalleePtr);
  auto *Result = Builder.CreateCall(Callee, Arguments);
  if (MFunction.getType().isVoidResult()) {
    Builder.CreateRetVoid();
  } else {
    Builder.CreateRet(Result);
  }
}

void EntityLayout::setupInitializationFunction() {
  auto &Context = Target.getContext();
  auto *InitializationTy = llvm::FunctionType::get(
      llvm::Type::getVoidTy(Context),
      {llvm::PointerType::getUnqual(getNamedStructTy("__sable_instance_t"))},
      false);

  auto *InitializationFn = llvm::Function::Create(
      /* Type    */ InitializationTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_initialize",
      /* Parent  */ Target);
  auto *EntryBasicBlock =
      llvm::BasicBlock::Create(Context, "entry", InitializationFn);
  llvm::IRBuilder<> Builder(EntryBasicBlock);

  auto *InstancePtr = InitializationFn->getArg(0);

  auto *MemoryMetadata_ = Target.getNamedGlobal("__sable_memory_metadata");
  auto *MemoryMetadata = Builder.CreateStructGEP(InstancePtr, 0);
  Builder.CreateStore(MemoryMetadata_, MemoryMetadata);
  auto *TableMetadata_ = Target.getNamedGlobal("__sable_table_metadata");
  auto *TableMetadata = Builder.CreateStructGEP(InstancePtr, 1);
  Builder.CreateStore(TableMetadata_, TableMetadata);
  auto *GlobalMetadata_ = Target.getNamedGlobal("__sable_global_metadata");
  auto *GlobalMetadata = Builder.CreateStructGEP(InstancePtr, 2);
  Builder.CreateStore(GlobalMetadata_, GlobalMetadata);
  auto *FunctionMetadata_ = Target.getNamedGlobal("__sable_function_metadata");
  auto *FunctionMetadata = Builder.CreateStructGEP(InstancePtr, 3);
  Builder.CreateStore(FunctionMetadata_, FunctionMetadata);

  for (auto const &MGlobal : Source.getGlobals()) {
    if (MGlobal.isImported()) continue;
    auto *GlobalPtr = get(Builder, InstancePtr, MGlobal);
    auto *Initializer =
        translateInitExpr(Builder, InstancePtr, *MGlobal.getInitializer());
    Builder.CreateStore(Initializer, GlobalPtr);
  }

  for (auto const &MFunction : Source.getFunctions()) {
    if (MFunction.isImported()) continue;
    auto *ErasedTy =
        llvm::PointerType::getUnqual(getOpaqueTy("__sable_function_t"));
    auto Offset = getOffset(MFunction);
    llvm::Value *FunctionPtr = Builder.CreateStructGEP(InstancePtr, Offset);
    FunctionPtr = Builder.CreateLoad(FunctionPtr);
    llvm::Value *Initializer = get(MFunction).Definition;
    Initializer = Builder.CreateBitCast(Initializer, ErasedTy);
    Builder.CreateStore(Initializer, FunctionPtr);
  }

  Builder.CreateRetVoid();
}

EntityLayout::EntityLayout(mir::Module const &Source_, llvm::Module &Target_)
    : Source(Source_), Target(Target_) {
  setupInstanceType();
  setupDataSegments();
  setupFunctions();
  setupMemoryMetadata();
  setupTableMetadata();
  setupGlobalMetadata();
  setupFunctionMetadata();
  setupInitializationFunction();
}

llvm::Value *EntityLayout::get(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::Global const &MGlobal) {
  auto Offset = getOffset(MGlobal);
  auto GlobalValueType = MGlobal.getType().getType();
  auto *CastedToTy = llvm::PointerType::getUnqual(convertType(GlobalValueType));
  llvm::Value *GlobalPtr = Builder.CreateStructGEP(InstancePtr, Offset);
  GlobalPtr = Builder.CreateLoad(GlobalPtr);
  GlobalPtr = Builder.CreateBitCast(GlobalPtr, CastedToTy);
  if (MGlobal.hasName()) GlobalPtr->setName(llvm::StringRef(MGlobal.getName()));
  return GlobalPtr;
}

llvm::Value *EntityLayout::get(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::Function const &MFunction) {
  auto Offset = getOffset(MFunction);
  auto *FunctionTy = convertType(MFunction.getType());
  llvm::Value *FunctionPtr = Builder.CreateStructGEP(InstancePtr, Offset);
  FunctionPtr = Builder.CreateLoad(FunctionPtr);
  FunctionPtr = Builder.CreateBitCast(
      FunctionPtr, llvm::PointerType::getUnqual(FunctionTy));
  if (MFunction.hasName())
    FunctionPtr->setName(llvm::StringRef(MFunction.getName()));
  return FunctionPtr;
}

} // namespace codegen::llvm_instance
