#include "LLVMCodege.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
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

namespace {
namespace detail {
struct InitExprTranslationVisitor :
    mir::InitExprVisitorBase<InitExprTranslationVisitor, llvm::Value *> {
  llvm::IRBuilder<> &Builder;
  llvm::Value *InstancePtr;
  EntityLayout &ELayout;
  InitExprTranslationVisitor(
      EntityLayout &ELayout_, llvm::IRBuilder<> &Builder_,
      llvm::Value *InstancePtr_)
      : Builder(Builder_), InstancePtr(InstancePtr_), ELayout(ELayout_) {}
  llvm::Value *operator()(mir::initializer::Constant const *InitExpr) {
    using VKind = bytecode::ValueTypeKind;
    switch (InitExpr->getValueType().getKind()) {
    case VKind::I32: return ELayout.getI32Constant(InitExpr->asI32());
    case VKind::I64: return ELayout.getI64Constant(InitExpr->asI64());
    case VKind::F32: return ELayout.getF32Constant(InitExpr->asF32());
    case VKind::F64: return ELayout.getF64Constant(InitExpr->asF64());
    default: utility::unreachable();
    }
  }
  llvm::Value *operator()(mir::initializer::GlobalGet const *InitExpr) {
    auto *TargetGlobal = InitExpr->getGlobalValue();
    auto *GlobalPtr = ELayout.get(Builder, InstancePtr, *TargetGlobal);
    auto *GlobalValue = Builder.CreateLoad(GlobalPtr);
    return GlobalValue;
  }
};
} // namespace detail
} // namespace

llvm::Value *EntityLayout::translateInitExpr(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::InitializerExpr const &Expr) {
  detail::InitExprTranslationVisitor Visitor(*this, Builder, InstancePtr);
  return Visitor.visit(std::addressof(Expr));
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
    std::array<llvm::Constant *, 2> Indices{
        getI32Constant(0), getI32Constant(0)};
    auto *DataPtr = llvm::ConstantExpr::getInBoundsGetElementPtr(
        DataGlobal->getValueType(), DataGlobal, Indices);
    DataMap.insert(std::make_pair(std::addressof(DataSegment), DataPtr));
  }
}

void EntityLayout::setupElementSegments() {
  auto *ErasedTy = getFunctionPtrTy();
  for (mir::ElementSegment const &ElementSegment : Source.getElements()) {
    // clang-format off
    auto Pointers = ElementSegment.getContent()
      | ranges::views::transform([&](mir::Function const *Function) {
          llvm::Constant * Pointer = this->operator[](*Function).definition();
          Pointer = llvm::ConstantExpr::getBitCast(Pointer, ErasedTy);
          return Pointer;
        })
      | ranges::to<std::vector<llvm::Constant *>>();
    auto TypeStrings = ElementSegment.getContent()
      | ranges::views::transform([&](mir::Function const *Function) {
          return this->operator[](*Function).typeString();
        })
      | ranges::to<std::vector<llvm::Constant *>>();
    // clang-format on
    auto *PointersTy = llvm::ArrayType::get(ErasedTy, Pointers.size());
    auto *PointersConstant = llvm::ConstantArray::get(PointersTy, Pointers);
    auto *TypeStringsConstant = llvm::ConstantArray::get(
        llvm::ArrayType::get(getCStringPtrTy(), TypeStrings.size()),
        TypeStrings);
    auto *PointersGlobal = new llvm::GlobalVariable(
        /* Parent      */ Target,
        /* Type        */ PointersConstant->getType(),
        /* IsConstant  */ true,
        /* Linkage     */ llvm::GlobalVariable::LinkageTypes::PrivateLinkage,
        /* Initializer */ PointersConstant,
        /* Name        */ "element.ptrs");
    auto *TypeStringsGlobal = new llvm::GlobalVariable(
        /* Parent      */ Target,
        /* Type        */ TypeStringsConstant->getType(),
        /* IsConstant  */ true,
        /* Linkage     */ llvm::GlobalVariable::LinkageTypes::PrivateLinkage,
        /* Initializer */ TypeStringsConstant,
        /* Name        */ "element.typestrs");
    PointersGlobal->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
    TypeStringsGlobal->setUnnamedAddr(
        llvm::GlobalVariable::UnnamedAddr::Global);
    std::array<llvm::Constant *, 2> Indices{
        getI32Constant(0), getI32Constant(0)};
    auto *PointersPtr = llvm::ConstantExpr::getInBoundsGetElementPtr(
        PointersGlobal->getValueType(), PointersGlobal, Indices);
    auto *TypeStringsPtr = llvm::ConstantExpr::getInBoundsGetElementPtr(
        TypeStringsGlobal->getValueType(), TypeStringsGlobal, Indices);
    ElementMap.insert(std::make_pair(
        std::addressof(ElementSegment),
        ElementEntry(PointersPtr, TypeStringsPtr)));
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

llvm::GlobalVariable *EntityLayout::setupMetadata(
    std::string_view Prefix, std::uint32_t Size, std::uint32_t ImportSize,
    std::uint32_t ExportSize, llvm::GlobalVariable *Entities,
    llvm::GlobalVariable *Imports, llvm::GlobalVariable *Exports) {
  Entities->setName(fmt::format("{}.entities", Prefix));
  Entities->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
  Imports->setName(fmt::format("{}.imports", Prefix));
  Imports->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
  Exports->setName(fmt::format("{}.exports", Prefix));
  Exports->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);

  auto TypeName = fmt::format("{}_t", Prefix);
  auto *MetadataTy = getNamedStructTy(TypeName);
  MetadataTy->setBody(
      {/* Size       */ getI32Ty(),
       /* ImportSize */ getI32Ty(),
       /* ExportSize */ getI32Ty(),
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

  auto *EntityTy = llvm::StructType::get(Context, {getI32Ty(), getI32Ty()});
  auto *ImportTy = llvm::StructType::get(
      Context, {getI32Ty(), getCStringPtrTy(), getCStringPtrTy()});
  auto *ExportTy =
      llvm::StructType::get(Context, {getI32Ty(), getCStringPtrTy()});
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

  setupMetadata(
      "__sable_memory_metadata",
      /* Sizes    */ Entities.size(), Imports.size(), Exports.size(),
      /* Entities */ setupArrayGlobal(EntityTy, Entities),
      /* Imports  */ setupArrayGlobal(ImportTy, Imports),
      /* Exports  */ setupArrayGlobal(ExportTy, Exports));
}

void EntityLayout::setupTableMetadata() {
  auto &Context = Target.getContext();

  auto *EntityTy = llvm::StructType::get(Context, {getI32Ty(), getI32Ty()});
  auto *ImportTy = llvm::StructType::get(
      Context, {getI32Ty(), getCStringPtrTy(), getCStringPtrTy()});
  auto *ExportTy =
      llvm::StructType::get(Context, {getI32Ty(), getCStringPtrTy()});
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

  setupMetadata(
      "__sable_table_metadata",
      /* Sizes    */ Entities.size(), Imports.size(), Exports.size(),
      /* Entities */ setupArrayGlobal(EntityTy, Entities),
      /* Imports  */ setupArrayGlobal(ImportTy, Imports),
      /* Exports  */ setupArrayGlobal(ExportTy, Exports));
}

void EntityLayout::setupGlobalMetadata() {
  auto &Context = Target.getContext();

  auto *ImportTy = llvm::StructType::get(
      Context, {getI32Ty(), getCStringPtrTy(), getCStringPtrTy()});
  auto *ExportTy =
      llvm::StructType::get(Context, {getI32Ty(), getCStringPtrTy()});
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
    default: utility::unreachable();
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

  setupMetadata(
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
      Context, {getI32Ty(), getCStringPtrTy(), getCStringPtrTy()});
  auto *ExportTy =
      llvm::StructType::get(Context, {getI32Ty(), getCStringPtrTy()});
  std::vector<llvm::Constant *> Entities;
  std::vector<llvm::Constant *> Imports;
  std::vector<llvm::Constant *> Exports;

  for (auto const &Function : Source.getFunctions())
    Entities.push_back(this->operator[](Function).typeString());
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

  setupMetadata(
      "__sable_function_metadata",
      /* Sizes    */ Entities.size(), Imports.size(), Exports.size(),
      /* Entities */ setupArrayGlobal(EntityTy, Entities),
      /* Imports  */ setupArrayGlobal(ImportTy, Imports),
      /* Exports  */ setupArrayGlobal(ExportTy, Exports));
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
        std::addressof(Function), FunctionEntry(Definition, TypeString)));
    if (Function.isImported()) {
      /* Setup import function forwarding */
      auto *EntryBasicBlock = llvm::BasicBlock::Create(
          /* Context */ Target.getContext(),
          /* Name    */ "entry",
          /* Parent  */ Definition);
      llvm::IRBuilder<> Builder(EntryBasicBlock);
      // clang-format off
      auto Arguments =
        ranges::subrange(Definition->arg_begin(), Definition->arg_end())
        | ranges::views::transform([](llvm::Argument &Argument) {
            return std::addressof(Argument);
          })
        | ranges::to<std::vector<llvm::Value *>>();
      // clang-format on
      auto Offset = getOffset(Function);
      auto *CalleeTy = convertType(Function.getType());
      auto *CalleePtrTy = llvm::PointerType::getUnqual(CalleeTy);
      llvm::Value *CalleePtr = Builder.CreateStructGEP(Arguments[0], Offset);
      CalleePtr = Builder.CreateLoad(CalleePtr);
      CalleePtr = Builder.CreateBitCast(CalleePtr, CalleePtrTy);
      llvm::FunctionCallee Callee(CalleeTy, CalleePtr);
      auto *Result = Builder.CreateCall(Callee, Arguments);
      if (Function.getType().isVoidResult()) {
        Builder.CreateRetVoid();
      } else {
        Builder.CreateRet(Result);
      }
    }
  }
}

void EntityLayout::setupInitialization() {
  auto &Context = Target.getContext();
  auto *InitializationTy =
      llvm::FunctionType::get(getVoidTy(), {getInstancePtrTy()}, false);

  auto *InitializationFn = llvm::Function::Create(
      /* Type    */ InitializationTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_initialize",
      /* Parent  */ Target);
  auto *EntryBasicBlock =
      llvm::BasicBlock::Create(Context, "entry", InitializationFn);
  llvm::IRBuilder<> Builder(EntryBasicBlock);

  auto *InstancePtr = InitializationFn->getArg(0);

  auto *MemoryMetadata = Builder.CreateStructGEP(InstancePtr, 0);
  Builder.CreateStore(getMemoryMetadata(), MemoryMetadata);
  auto *TableMetadata = Builder.CreateStructGEP(InstancePtr, 1);
  Builder.CreateStore(getTableMetadata(), TableMetadata);
  auto *GlobalMetadata = Builder.CreateStructGEP(InstancePtr, 2);
  Builder.CreateStore(getGlobalMetadata(), GlobalMetadata);
  auto *FunctionMetadata = Builder.CreateStructGEP(InstancePtr, 3);
  Builder.CreateStore(getFunctionMetadata(), FunctionMetadata);

  for (auto const &Memory : Source.getMemories()) {
    for (auto const *DataSegment : Memory.getInitializers()) {
      auto *Data = this->operator[](*DataSegment);
      auto *MemoryBase = get(Builder, InstancePtr, Memory);
      llvm::Value *Offset =
          translateInitExpr(Builder, InstancePtr, *DataSegment->getOffset());

      auto *GuardAddress =
          Builder.CreateAdd(Offset, getI32Constant(DataSegment->getSize()));
      Builder.CreateCall(
          getBuiltin("__sable_memory_guard"), {MemoryBase, GuardAddress});

      Offset = Builder.CreateZExtOrTrunc(Offset, getPtrIntTy());
      llvm::Value *Dest = Builder.CreatePtrToInt(MemoryBase, getPtrIntTy());
      Dest = Builder.CreateAdd(Dest, Offset);
      Dest = Builder.CreateIntToPtr(Dest, Builder.getInt8PtrTy());
      auto *Length = Builder.getInt32(DataSegment->getSize());
      auto *IsVolatile = Builder.getFalse();
      auto *Intrinsic = llvm::Intrinsic::getDeclaration(
          std::addressof(Target), llvm::Intrinsic::memcpy,
          std::array<llvm::Type *, 4>{
              Dest->getType(), Data->getType(), Length->getType(),
              IsVolatile->getType()});
      Builder.CreateCall(Intrinsic, {Dest, Data, Length, IsVolatile});
    }
  }

  for (mir::Table const &Table : Source.getTables()) {
    for (mir::ElementSegment const *ElementSegment : Table.getInitializers()) {
      auto *Pointers = this->operator[](*ElementSegment).pointers();
      auto *TypeStrings = this->operator[](*ElementSegment).typeStrings();
      auto *TableBase = get(Builder, InstancePtr, Table);
      llvm::Value *Offset =
          translateInitExpr(Builder, InstancePtr, *ElementSegment->getOffset());

      auto *GuardIndex =
          Builder.CreateAdd(Offset, getI32Constant(ElementSegment->getSize()));
      Builder.CreateCall(
          getBuiltin("__sable_table_guard"), {TableBase, GuardIndex});

      Builder.CreateCall(
          getBuiltin("__sable_table_set"),
          {TableBase, Offset, getI32Constant(ElementSegment->getSize()),
           Pointers, TypeStrings});
    }
  }

  for (auto const &MGlobal : Source.getGlobals()) {
    if (MGlobal.isImported()) continue;
    auto *GlobalPtr = get(Builder, InstancePtr, MGlobal);
    auto *Initializer =
        translateInitExpr(Builder, InstancePtr, *MGlobal.getInitializer());
    Builder.CreateStore(Initializer, GlobalPtr);
  }

  for (auto const &MFunction : Source.getFunctions()) {
    if (MFunction.isImported()) continue;
    auto Offset = getOffset(MFunction);
    llvm::Value *FunctionFieldPtr =
        Builder.CreateStructGEP(InstancePtr, Offset);
    llvm::Value *Initializer = this->operator[](MFunction).definition();
    Initializer = Builder.CreateBitCast(Initializer, getFunctionPtrTy());
    Builder.CreateStore(Initializer, FunctionFieldPtr);
  }

  Builder.CreateRetVoid();
}

void EntityLayout::setupBuiltins() {
  /* void __sable_memory_guard(__sable_memory_t * mem, std::uint32_t address) */
  auto *MemoryGuardTy = llvm::FunctionType::get(
      getVoidTy(), {getMemoryPtrTy(), getI32Ty()}, false);
  llvm::Function::Create(
      /* Type    */ MemoryGuardTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_memory_guard",
      /* Parent  */ Target);

  /* void __sable_table_guard(__sable_table_t *table, std::uint32_t index) */
  auto *TableGuardTy = llvm::FunctionType::get(
      getVoidTy(), {getTablePtrTy(), getI32Ty()}, false);
  llvm::Function::Create(
      /* Type    */ TableGuardTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_table_guard",
      /* Parent  */ Target);

  /* void __sable_table_set(
   *    __sable_table_t *table, std::uint32_t start, std::uint32_t size,
   *    __sable_function_t *pointers[], char const * types[])    */
  auto *TableSetTy = llvm::FunctionType::get(
      getVoidTy(),
      {getTablePtrTy(), getI32Ty(), getI32Ty(),
       llvm::PointerType::getUnqual(getFunctionPtrTy()),
       llvm::PointerType::getUnqual(getCStringPtrTy())},
      false);
  llvm::Function::Create(
      /* Type    */ TableSetTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_table_set",
      /* Parent  */ Target);

  /* __sable_function_t * __sable_table_get(
   *   __sable_table_t *table, std::uint32_t index, char const * expect_type) */
  auto *TableGetTy = llvm::FunctionType::get(
      getFunctionPtrTy(), {getTablePtrTy(), getI32Ty(), getCStringPtrTy()},
      false);
  llvm::Function::Create(
      /* Type    */ TableGetTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_table_get",
      /* Parent  */ Target);

  /* void __sable_unreachable() */
  auto *UnreachableTy = llvm::FunctionType::get(getVoidTy(), {}, false);
  llvm::Function::Create(
      /* Type    */ UnreachableTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_unreachable",
      /* Parent  */ Target);
}

EntityLayout::EntityLayout(mir::Module const &Source_, llvm::Module &Target_)
    : Source(Source_), Target(Target_) {
  setupInstanceType();
  setupBuiltins();
  setupFunctions();
  setupDataSegments();
  setupElementSegments();
  setupMemoryMetadata();
  setupTableMetadata();
  setupGlobalMetadata();
  setupFunctionMetadata();
  setupInitialization();
}

llvm::Type *EntityLayout::convertType(bytecode::ValueType const &Type) {
  switch (Type.getKind()) {
  case bytecode::ValueTypeKind::I32: return getI32Ty();
  case bytecode::ValueTypeKind::I64: return getI64Ty();
  case bytecode::ValueTypeKind::F32: return getF32Ty();
  case bytecode::ValueTypeKind::F64: return getF64Ty();
  default: utility::unreachable();
  }
}

llvm::FunctionType *
EntityLayout::convertType(bytecode::FunctionType const &Type) {
  auto &Context = Target.getContext();
  std::vector<llvm::Type *> ParamTypes;
  ParamTypes.reserve(Type.getNumParameter() + 1);
  ParamTypes.push_back(getInstancePtrTy());
  for (auto const &ValueType : Type.getParamTypes())
    ParamTypes.push_back(convertType(ValueType));
  if (Type.isVoidResult())
    return llvm::FunctionType::get(getVoidTy(), ParamTypes, false);
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
  utility::unreachable();
}

std::size_t EntityLayout::getOffset(mir::ASTNode const &Node) const {
  auto SearchIter = OffsetMap.find(std::addressof(Node));
  assert(SearchIter != OffsetMap.end());
  return std::get<1>(*SearchIter) + 4;
}

llvm::Constant *
EntityLayout::operator[](mir::DataSegment const &DataSegment) const {
  auto SearchIter = DataMap.find(std::addressof(DataSegment));
  assert(SearchIter != DataMap.end());
  return std::get<1>(*SearchIter);
}

EntityLayout::FunctionEntry const &
EntityLayout::operator[](mir::Function const &Function) const {
  auto SearchIter = FunctionMap.find(std::addressof(Function));
  assert(SearchIter != FunctionMap.end());
  return std::get<1>(*SearchIter);
}

EntityLayout::ElementEntry const &
EntityLayout::operator[](mir::ElementSegment const &ElementSegment) const {
  auto SearchIter = ElementMap.find(std::addressof(ElementSegment));
  assert(SearchIter != ElementMap.end());
  return std::get<1>(*SearchIter);
}

llvm::Function *EntityLayout::getBuiltin(std::string_view Name) {
  auto *Builtin = Target.getFunction(Name);
  assert(Builtin != nullptr);
  return Builtin;
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

llvm::Value *EntityLayout::get(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::Memory const &MMemory) {
  auto Offset = getOffset(MMemory);
  llvm::Value *MemoryPtr = Builder.CreateStructGEP(InstancePtr, Offset);
  MemoryPtr = Builder.CreateLoad(MemoryPtr);
  if (MMemory.hasName()) MemoryPtr->setName(llvm::StringRef(MMemory.getName()));
  return MemoryPtr;
}

llvm::Value *EntityLayout::get(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    const mir::Table &MTable) {
  auto Offset = getOffset(MTable);
  llvm::Value *TablePtr = Builder.CreateStructGEP(InstancePtr, Offset);
  TablePtr = Builder.CreateLoad(TablePtr);
  if (MTable.hasName()) TablePtr->setName(llvm::StringRef(MTable.getName()));
  return TablePtr;
}

char EntityLayout::getTypeChar(bytecode::ValueType const &Type) {
  switch (Type.getKind()) {
  case bytecode::ValueTypeKind::I32: return 'I';
  case bytecode::ValueTypeKind::I64: return 'J';
  case bytecode::ValueTypeKind::F32: return 'F';
  case bytecode::ValueTypeKind::F64: return 'D';
  default: utility::unreachable();
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

llvm::Type *EntityLayout::getVoidTy() {
  return llvm::Type::getVoidTy(Target.getContext());
}

llvm::PointerType *EntityLayout::getVoidPtrTy() {
  return llvm::Type::getInt8PtrTy(Target.getContext());
}

llvm::PointerType *EntityLayout::getCStringPtrTy() {
  return llvm::Type::getInt8PtrTy(Target.getContext());
}

llvm::Constant *
EntityLayout::getCStringPtr(std::string_view Content, std::string_view Name) {
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
  auto *Zero = getI32Constant(0);
  std::array<llvm::Constant *, 2> Indices{Zero, Zero};
  auto *CastedPtr = llvm::dyn_cast<llvm::GlobalVariable>(CString);
  return llvm::ConstantExpr::getInBoundsGetElementPtr(
      CastedPtr->getValueType(), CString, Indices);
}

llvm::IntegerType *EntityLayout::getI32Ty() {
  return llvm::Type::getInt32Ty(Target.getContext());
}

llvm::Constant *EntityLayout::getI32Constant(std::int32_t Value) {
  return llvm::ConstantInt::get(getI32Ty(), Value);
}

llvm::IntegerType *EntityLayout::getI64Ty() {
  return llvm::Type::getInt64Ty(Target.getContext());
}

llvm::Constant *EntityLayout::getI64Constant(std::int64_t Value) {
  return llvm::ConstantInt::get(getI64Ty(), Value);
}

llvm::Type *EntityLayout::getF32Ty() {
  return llvm::Type::getFloatTy(Target.getContext());
}

llvm::Constant *EntityLayout::getF32Constant(float Value) {
  return llvm::ConstantFP::get(getF32Ty(), Value);
}

llvm::Type *EntityLayout::getF64Ty() {
  return llvm::Type::getDoubleTy(Target.getContext());
}

llvm::Constant *EntityLayout::getF64Constant(double Value) {
  return llvm::ConstantFP::get(getF64Ty(), Value);
}

llvm::Type *EntityLayout::getPtrIntTy() {
  auto &TargetDataLayout = Target.getDataLayout();
  return TargetDataLayout.getIntPtrType(Target.getContext());
}

llvm::PointerType *EntityLayout::getInstancePtrTy() {
  auto *InstanceTy = getNamedStructTy("__sable_instance_t");
  return llvm::PointerType::getUnqual(InstanceTy);
}

llvm::StructType *EntityLayout::getMemoryMetadataTy() {
  return getNamedStructTy("__sable_memory_metadata_t");
}

llvm::StructType *EntityLayout::getTableMetadataTy() {
  return getNamedStructTy("__sable_table_metadata_t");
}

llvm::StructType *EntityLayout::getGlobalMetadataTy() {
  return getNamedStructTy("__sable_global_metadata_t");
}

llvm::StructType *EntityLayout::getFunctionMetadataTy() {
  return getNamedStructTy("__sable_function_metadata_t");
}

llvm::PointerType *EntityLayout::getMemoryPtrTy() {
  auto *OpaqueTy = getOpaqueTy("__sable_memory_t");
  return llvm::PointerType::getUnqual(OpaqueTy);
}

llvm::PointerType *EntityLayout::getTablePtrTy() {
  auto *OpaqueTy = getOpaqueTy("__sable_table_t");
  return llvm::PointerType::getUnqual(OpaqueTy);
}

llvm::PointerType *EntityLayout::getGlobalPtrTy() {
  auto *OpaqueTy = getOpaqueTy("__sable_global_t");
  return llvm::PointerType::getUnqual(OpaqueTy);
}

llvm::PointerType *EntityLayout::getFunctionPtrTy() {
  auto *OpaqueTy = getOpaqueTy("__sable_function_t");
  return llvm::PointerType::getUnqual(OpaqueTy);
}

llvm::GlobalVariable *EntityLayout::getMemoryMetadata() {
  return Target.getGlobalVariable("__sable_memory_metadata");
}

llvm::GlobalVariable *EntityLayout::getTableMetadata() {
  return Target.getGlobalVariable("__sable_table_metadata");
}

llvm::GlobalVariable *EntityLayout::getGlobalMetadata() {
  return Target.getGlobalVariable("__sable_global_metadata");
}

llvm::GlobalVariable *EntityLayout::getFunctionMetadata() {
  return Target.getGlobalVariable("__sable_function_metadata");
}
} // namespace codegen::llvm_instance
