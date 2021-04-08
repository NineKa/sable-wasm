#include "../mir/Function.h"
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

#define INSTANCE_ENTITY_START_OFFSET 4

namespace codegen::llvm_instance {

llvm::StructType *EntityLayout::declareOpaqueTy(std::string_view Name) {
  return llvm::StructType::create(Target.getContext(), Name);
}

llvm::StructType *EntityLayout::createNamedStructTy(std::string_view Name) {
  return llvm::StructType::create(Target.getContext(), Name);
}

llvm::StructType *EntityLayout::getNamedStructTy(std::string_view Name) const {
  auto *Type = Target.getTypeByName(Name);
  assert((Type != nullptr) && (llvm::isa<llvm::StructType>(Type)));
  return llvm::dyn_cast<llvm::StructType>(Type);
}

llvm::StructType *EntityLayout::getOpaqueTy(std::string_view Name) const {
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
  assert(InstanceFields.size() == INSTANCE_ENTITY_START_OFFSET);

  auto *MemoryOpaqueTy = declareOpaqueTy("__sable_memory_t");
  auto *MemoryOpaquePtrTy = llvm::PointerType::getUnqual(MemoryOpaqueTy);
  for (auto const &Memory : Source.getMemories().asView()) {
    OffsetMap.insert(std::make_pair(std::addressof(Memory), OffsetMap.size()));
    InstanceFields.push_back(MemoryOpaquePtrTy);
  }

  auto *TableOpaqueTy = declareOpaqueTy("__sable_table_t");
  auto *TableOpaquePtrTy = llvm::PointerType::getUnqual(TableOpaqueTy);
  for (auto const &Table : Source.getTables().asView()) {
    OffsetMap.insert(std::make_pair(std::addressof(Table), OffsetMap.size()));
    InstanceFields.push_back(TableOpaquePtrTy);
  }

  auto *GlobalOpaqueTy = declareOpaqueTy("__sable_global_t");
  auto *GlobalOpaquePtrTy = llvm::PointerType::getUnqual(GlobalOpaqueTy);
  for (auto const &Global : Source.getGlobals().asView()) {
    OffsetMap.insert(std::make_pair(std::addressof(Global), OffsetMap.size()));
    InstanceFields.push_back(GlobalOpaquePtrTy);
  }

  auto FunctionIndex = OffsetMap.size();
  auto *FunctionOpaqueTy = declareOpaqueTy("__sable_function_t");
  auto *FunctionOpaquePtrTy = llvm::PointerType::getUnqual(FunctionOpaqueTy);
  for (auto const &Function : Source.getFunctions().asView()) {
    auto *FunctionAddress = std::addressof(Function);
    OffsetMap.insert(std::make_pair(FunctionAddress, FunctionIndex));
    InstanceFields.push_back(llvm::PointerType::getUnqual(InstanceTy));
    InstanceFields.push_back(FunctionOpaquePtrTy);
    FunctionIndex = FunctionIndex + 2;
  }

  InstanceTy->setBody(InstanceFields);
}

namespace {
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
} // namespace

llvm::Value *EntityLayout::translateInitExpr(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::InitializerExpr const &Expr) {
  InitExprTranslationVisitor Visitor(*this, Builder, InstancePtr);
  return Visitor.visit(std::addressof(Expr));
}

void EntityLayout::setupDataSegments() {
  auto &Context = Target.getContext();
  for (auto const &DataSegment : Source.getData().asView()) {
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
    auto *Zero = getI32Constant(0);
    std::array<llvm::Constant *, 2> Indices{Zero, Zero};
    auto *DataPtr = llvm::ConstantExpr::getInBoundsGetElementPtr(
        DataGlobal->getValueType(), DataGlobal, Indices);
    DataMap.insert(std::make_pair(std::addressof(DataSegment), DataPtr));
  }
}

void EntityLayout::setupElementSegments() {
  for (auto const &ElementSegment : Source.getElements().asView()) {
    // clang-format off
    auto Indices = ElementSegment.getContent()
      | ranges::views::transform([&](mir::Function const *Function) {
          auto Index = this->operator[](*Function).index();
          return getI32Constant(Index);
        })
      | ranges::to<std::vector<llvm::Constant *>>();
    // clang-format on
    auto *IndicesTy = llvm::ArrayType::get(getI32Ty(), Indices.size());
    auto *IndicesConstant = llvm::ConstantArray::get(IndicesTy, Indices);
    auto *IndicesGlobal = new llvm::GlobalVariable(
        /* Parent      */ Target,
        /* Type        */ IndicesConstant->getType(),
        /* IsConstant  */ true,
        /* Linkage     */ llvm::GlobalVariable::LinkageTypes::PrivateLinkage,
        /* Initializer */ IndicesConstant,
        /* Name        */ "element");
    IndicesGlobal->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
    auto *Zero = getI32Constant(0);
    std::array<llvm::Constant *, 2> GEPIndices{Zero, Zero};
    auto *OffsetsPtr = llvm::ConstantExpr::getInBoundsGetElementPtr(
        IndicesGlobal->getValueType(), IndicesGlobal, GEPIndices);
    ElementMap.insert(
        std::make_pair(std::addressof(ElementSegment), OffsetsPtr));
  }
}

llvm::GlobalVariable *EntityLayout::createArrayGlobal(
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

llvm::GlobalVariable *EntityLayout::createMetadata(
    std::string_view Prefix, std::uint32_t Size, std::uint32_t ImportSize,
    std::uint32_t ExportSize, llvm::GlobalVariable *Signatures,
    llvm::GlobalVariable *Imports, llvm::GlobalVariable *Exports) {
  Signatures->setName(fmt::format("{}.signatures", Prefix));
  Signatures->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
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
       /* Signatures */ Signatures->getType(),
       /* Imports    */ Imports->getType(),
       /* Exports    */ Exports->getType()});

  auto *MetadataConstant = llvm::ConstantStruct::get(
      MetadataTy, {/* Size       */ getI32Constant(Size),
                   /* ImportSize */ getI32Constant(ImportSize),
                   /* ExportSize */ getI32Constant(ExportSize),
                   /* Signatures */ Signatures,
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

  auto *SignatureTy = llvm::StructType::get(
      Context, {/* Min */ getI32Ty(), /* Max */ getI32Ty()});
  auto *ImportTy = llvm::StructType::get(
      Context, {/* Index */ getI32Ty(), /* ModuleName */ getCStringPtrTy(),
                /* EntityName */ getCStringPtrTy()});
  auto *ExportTy = llvm::StructType::get(
      Context, {/* Index */ getI32Ty(), /* Name */ getCStringPtrTy()});

  std::vector<llvm::Constant *> Signatures;
  std::vector<llvm::Constant *> Imports;
  std::vector<llvm::Constant *> Exports;

  for (auto const &Memory : Source.getMemories().asView()) {
    auto Min = Memory.getType().getMin();
    auto Max = Memory.getType().hasMax()
                   ? Memory.getType().getMax()
                   : std::numeric_limits<std::uint32_t>::max();
    auto *SignatureConstant = llvm::ConstantStruct::get(
        SignatureTy, {getI32Constant(Min), getI32Constant(Max)});
    Signatures.push_back(SignatureConstant);
  }

  for (auto const &[Index, Memory] :
       ranges::views::enumerate(Source.getMemories().asView())) {
    if (!Memory.isImported()) continue;
    auto *ModuleName = getCStringPtr(Memory.getImportModuleName());
    auto *EntityName = getCStringPtr(Memory.getImportEntityName());
    auto *ImportConstant = llvm::ConstantStruct::get(
        ImportTy, {getI32Constant(Index), ModuleName, EntityName});
    Imports.push_back(ImportConstant);
  }

  for (auto const &[Index, Memory] :
       ranges::views::enumerate(Source.getMemories().asView())) {
    if (!Memory.isExported()) continue;
    auto *EntityName = getCStringPtr(Memory.getExportName());
    auto *ExportConstant = llvm::ConstantStruct::get(
        ExportTy, {getI32Constant(Index), EntityName});
    Exports.push_back(ExportConstant);
  }

  createMetadata(
      "__sable_memory_metadata",
      /* Sizes      */ Signatures.size(), Imports.size(), Exports.size(),
      /* Signatures */ createArrayGlobal(SignatureTy, Signatures),
      /* Imports    */ createArrayGlobal(ImportTy, Imports),
      /* Exports    */ createArrayGlobal(ExportTy, Exports));
}

void EntityLayout::setupTableMetadata() {
  auto &Context = Target.getContext();

  auto *SignatureTy = llvm::StructType::get(
      Context, {/* Min */ getI32Ty(), /* Max */ getI32Ty()});
  auto *ImportTy = llvm::StructType::get(
      Context, {/* Index */ getI32Ty(), /* ModuleName */ getCStringPtrTy(),
                /* EntityName */ getCStringPtrTy()});
  auto *ExportTy = llvm::StructType::get(
      Context, {/* Index */ getI32Ty(), /* Name */ getCStringPtrTy()});

  std::vector<llvm::Constant *> Signatures;
  std::vector<llvm::Constant *> Imports;
  std::vector<llvm::Constant *> Exports;

  for (auto const &Table : Source.getTables().asView()) {
    auto Min = Table.getType().getMin();
    auto Max = Table.getType().hasMax()
                   ? Table.getType().getMin()
                   : std::numeric_limits<std::uint32_t>::max();
    auto *SignatureConstant = llvm::ConstantStruct::get(
        SignatureTy, {getI32Constant(Min), getI32Constant(Max)});
    Signatures.push_back(SignatureConstant);
  }

  for (auto const &[Index, Table] :
       ranges::views::enumerate(Source.getTables().asView())) {
    if (!Table.isImported()) continue;
    auto *ModuleName = getCStringPtr(Table.getImportModuleName());
    auto *EntityName = getCStringPtr(Table.getImportEntityName());
    auto *ImportConstant = llvm::ConstantStruct::get(
        ImportTy, {getI32Constant(Index), ModuleName, EntityName});
    Imports.push_back(ImportConstant);
  }

  for (auto const &[Index, Table] :
       ranges::views::enumerate(Source.getTables().asView())) {
    if (!Table.isExported()) continue;
    auto *EntityName = getCStringPtr(Table.getExportName());
    auto *ExportConstant = llvm::ConstantStruct::get(
        ExportTy, {getI32Constant(Index), EntityName});
    Exports.push_back(ExportConstant);
  }

  createMetadata(
      "__sable_table_metadata",
      /* Sizes      */ Signatures.size(), Imports.size(), Exports.size(),
      /* Signatures */ createArrayGlobal(SignatureTy, Signatures),
      /* Imports    */ createArrayGlobal(ImportTy, Imports),
      /* Exports    */ createArrayGlobal(ExportTy, Exports));
}

void EntityLayout::setupGlobalMetadata() {
  auto &Context = Target.getContext();

  auto *ImportTy = llvm::StructType::get(
      Context, {/* Index */ getI32Ty(), /* ModuleName */ getCStringPtrTy(),
                /* EntityName */ getCStringPtrTy()});
  auto *ExportTy = llvm::StructType::get(
      Context, {/* Index */ getI32Ty(), /* Name */ getCStringPtrTy()});

  std::string Signatures;
  std::vector<llvm::Constant *> Imports;
  std::vector<llvm::Constant *> Exports;

  for (auto const &Global : Source.getGlobals().asView()) {
    auto Signature = getSignature(Global.getType());
    Signatures.push_back(Signature);
  }

  for (auto const &[Index, Global] :
       ranges::views::enumerate(Source.getGlobals().asView())) {
    if (!Global.isImported()) continue;
    auto *ModuleName = getCStringPtr(Global.getImportModuleName());
    auto *EntityName = getCStringPtr(Global.getImportEntityName());
    auto *ImportConstant = llvm::ConstantStruct::get(
        ImportTy, {getI32Constant(Index), ModuleName, EntityName});
    Imports.push_back(ImportConstant);
  }

  for (auto const &[Index, Global] :
       ranges::views::enumerate(Source.getGlobals().asView())) {
    if (!Global.isExported()) continue;
    auto *EntityName = getCStringPtr(Global.getExportName());
    auto *ExportConstant = llvm::ConstantStruct::get(
        ExportTy, {getI32Constant(Index), EntityName});
    Exports.push_back(ExportConstant);
  }

  llvm::Constant *SignaturesConstant =
      llvm::ConstantDataArray::getString(Context, Signatures, false);

  auto *SignaturesGlobal = new llvm::GlobalVariable(
      /* Parent      */ Target,
      /* Type        */ SignaturesConstant->getType(),
      /* IsConstant  */ true,
      /* Linkage     */ llvm::GlobalVariable::LinkageTypes::PrivateLinkage,
      /* Initializer */ SignaturesConstant);

  createMetadata(
      "__sable_global_metadata",
      /* Sizes      */ Signatures.size(), Imports.size(), Exports.size(),
      /* Signatures */ SignaturesGlobal,
      /* Imports    */ createArrayGlobal(ImportTy, Imports),
      /* Exports    */ createArrayGlobal(ExportTy, Exports));
}

void EntityLayout::setupFunctionMetadata() {
  auto &Context = Target.getContext();

  auto *SignatureTy = getCStringPtrTy();
  auto *ImportTy = llvm::StructType::get(
      Context, {/* Index */ getI32Ty(), /* ModuleName */ getCStringPtrTy(),
                /* EntityName */ getCStringPtrTy()});
  auto *ExportTy = llvm::StructType::get(
      Context, {/* Index */ getI32Ty(), /* Name */ getCStringPtrTy()});

  std::vector<llvm::Constant *> Signatures;
  std::vector<llvm::Constant *> Imports;
  std::vector<llvm::Constant *> Exports;

  for (auto const &Function : Source.getFunctions().asView())
    Signatures.push_back(this->operator[](Function).signature());

  for (auto const &[Index, Function] :
       ranges::views::enumerate(Source.getFunctions().asView())) {
    if (!Function.isImported()) continue;
    auto *ModuleName = getCStringPtr(Function.getImportModuleName());
    auto *EntityName = getCStringPtr(Function.getImportEntityName());
    auto *ImportConstant = llvm::ConstantStruct::get(
        ImportTy, {getI32Constant(Index), ModuleName, EntityName});
    Imports.push_back(ImportConstant);
  }

  for (auto const &[Index, Function] :
       ranges::views::enumerate(Source.getFunctions().asView())) {
    if (!Function.isExported()) continue;
    auto *EntityName = getCStringPtr(Function.getExportName());
    auto *ExportConstant = llvm::ConstantStruct::get(
        ExportTy, {getI32Constant(Index), EntityName});
    Exports.push_back(ExportConstant);
  }

  createMetadata(
      "__sable_function_metadata",
      /* Sizes    */ Signatures.size(), Imports.size(), Exports.size(),
      /* Entities */ createArrayGlobal(SignatureTy, Signatures),
      /* Imports  */ createArrayGlobal(ImportTy, Imports),
      /* Exports  */ createArrayGlobal(ExportTy, Exports));
}

void EntityLayout::setupFunctions() {
  for (auto const &[Index, Function] :
       ranges::views::enumerate(Source.getFunctions().asView())) {
    auto *SignatureStr = getCStringPtr(
        getSignature(Function.getType()),
        Function.hasName() ? fmt::format("signature.{}", Function.getName())
                           : "signature");
    auto *Definition = llvm::Function::Create(
        /* Type    */ convertType(Function.getType()),
        /* Linkage */ llvm::GlobalVariable::PrivateLinkage,
        /* Name    */ llvm::StringRef(Function.getName()),
        /* Parent  */ Target);
    FunctionMap.insert(std::make_pair(
        std::addressof(Function),
        FunctionEntry(Index, Definition, SignatureStr)));
    if (Function.isImported()) {
      /* Setup import function forwarding */
      auto *EntryBasicBlock = llvm::BasicBlock::Create(
          /* Context */ Target.getContext(),
          /* Name    */ "entry",
          /* Parent  */ Definition);
      llvm::IRBuilder<> Builder(EntryBasicBlock);
      auto *InstancePtr = Definition->arg_begin();
      auto *ContextPtr = getContextPtr(Builder, InstancePtr, Function);
      auto *FunctionPtr = getFunctionPtr(Builder, InstancePtr, Function);
      auto *IsNullTest = Builder.CreateIsNull(ContextPtr);
      ContextPtr = Builder.CreateSelect(IsNullTest, InstancePtr, ContextPtr);
      // clang-format off
      auto Arguments =
        ranges::subrange(Definition->arg_begin(), Definition->arg_end())
        | ranges::views::transform([](llvm::Argument &Argument) {
            return std::addressof(Argument);
          })
        | ranges::to<std::vector<llvm::Value *>>();
      // clang-format on
      Arguments[0] = ContextPtr;
      auto *CalleeTy = convertType(Function.getType());
      llvm::FunctionCallee Callee(CalleeTy, FunctionPtr);
      auto *ForwardResult = Builder.CreateCall(Callee, Arguments);
      if (Function.getType().isVoidResult()) {
        Builder.CreateRetVoid();
      } else {
        Builder.CreateRet(ForwardResult);
      }
    }
  }
}

void EntityLayout::setupInitializer() {
  auto &Context = Target.getContext();

  auto *InitializerTy =
      llvm::FunctionType::get(getVoidTy(), {getInstancePtrTy()}, false);
  auto *InitializerFn = llvm::Function::Create(
      /* Type    */ InitializerTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_initialize",
      /* Parent  */ Target);

  auto *EntryBasicBlock =
      llvm::BasicBlock::Create(Context, "entry", InitializerFn);
  llvm::IRBuilder<> Builder(EntryBasicBlock);

  auto *InstancePtr = InitializerFn->getArg(0);

  for (auto const &Memory : Source.getMemories().asView()) {
    for (auto const *DataSegment : Memory.getInitializers()) {
      auto *Data = this->operator[](*DataSegment);
      auto *MemoryInstance = get(Builder, InstancePtr, Memory);
      llvm::Value *Offset =
          translateInitExpr(Builder, InstancePtr, *DataSegment->getOffset());

      if (!Options.SkipMemBoundaryCheck) {
        auto *GuardAddress =
            Builder.CreateAdd(Offset, getI32Constant(DataSegment->getSize()));
        Builder.CreateCall(
            getBuiltin("__sable_memory_guard"), {MemoryInstance, GuardAddress});
      }

      Offset = Builder.CreateZExtOrTrunc(Offset, getPtrIntTy());
      llvm::Value *Dest = Builder.CreatePtrToInt(MemoryInstance, getPtrIntTy());
      Dest = Builder.CreateAdd(Dest, Offset);
      Dest = Builder.CreateIntToPtr(Dest, Builder.getInt8PtrTy());
      auto *Length = getI32Constant(DataSegment->getSize());
      auto *IsVolatile = Builder.getFalse();
      auto *Intrinsic = llvm::Intrinsic::getDeclaration(
          std::addressof(Target), llvm::Intrinsic::memcpy,
          std::array<llvm::Type *, 4>{
              Dest->getType(), Data->getType(), Length->getType(),
              IsVolatile->getType()});
      Builder.CreateCall(Intrinsic, {Dest, Data, Length, IsVolatile});
    }
  }

  for (auto const &Global : Source.getGlobals().asView()) {
    if (Global.isImported()) continue;
    auto *GlobalInstance = get(Builder, InstancePtr, Global);
    auto *Initializer =
        translateInitExpr(Builder, InstancePtr, *Global.getInitializer());
    Builder.CreateStore(Initializer, GlobalInstance);
  }

  for (auto const &Function : Source.getFunctions().asView()) {
    if (Function.isImported()) continue;
    auto Offset = getOffset(Function);
    auto *ContextPtrAddr = Builder.CreateStructGEP(InstancePtr, Offset);
    auto *FunctionPtrAddr = Builder.CreateStructGEP(InstancePtr, Offset + 1);
    auto *ContextPtrInitializer = InstancePtr;
    auto *FunctionPtrInitializer = Builder.CreatePointerCast(
        this->operator[](Function).definition(), getFunctionPtrTy());
    Builder.CreateStore(ContextPtrInitializer, ContextPtrAddr);
    Builder.CreateStore(FunctionPtrInitializer, FunctionPtrAddr);
  }

  for (auto const &Table : Source.getTables().asView()) {
    for (auto const *ElementSegment : Table.getInitializers()) {
      auto *Indices = this->operator[](*ElementSegment);
      auto *TableInstance = get(Builder, InstancePtr, Table);
      llvm::Value *Offset =
          translateInitExpr(Builder, InstancePtr, *ElementSegment->getOffset());

      if (!Options.SkipTblBoundaryCheck) {
        auto *GuardIndex = Builder.CreateAdd(
            Offset, getI32Constant(ElementSegment->getSize()));
        Builder.CreateCall(
            getBuiltin("__sable_table_guard"), {TableInstance, GuardIndex});
      }

      Builder.CreateCall(
          getBuiltin("__sable_table_set"),
          {TableInstance, InstancePtr, Offset,
           getI32Constant(ElementSegment->getSize()), Indices});
    }
  }

  Builder.CreateRetVoid();
}

void EntityLayout::setupBuiltins() {
  if (!Options.SkipMemBoundaryCheck) {
    auto *MemoryGuardFnTy = llvm::FunctionType::get(
        getVoidTy(),
        {/* __sable_memory_t *memory */ getMemoryPtrTy(),
         /* std::uint32_t    offset  */ getI32Ty()},
        false);
    llvm::Function::Create(
        /* Type    */ MemoryGuardFnTy,
        /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
        /* Name    */ "__sable_memory_guard",
        /* Parent  */ Target);
  }

  auto *MemoryGrowFnTy = llvm::FunctionType::get(
      /* std::uint32_t       num_page_after_grow */ getI32Ty(),
      {/* __sable_memory_t   *memory             */ getMemoryPtrTy(),
       /* std::uint32_t      num_page_delta      */ getI32Ty()},
      false);
  llvm::Function::Create(
      /* Type    */ MemoryGrowFnTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_memory_grow",
      /* Parent  */ Target);

  auto *MemorySizeFnTy = llvm::FunctionType::get(
      /* std::uint32_t     num_page */ getI32Ty(),
      {/* __sable_memory_t *memory  */ getMemoryPtrTy()}, false);
  llvm::Function::Create(
      /* Type    */ MemorySizeFnTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_memory_size",
      /* Parent  */ Target);

  if (!Options.SkipTblBoundaryCheck) {
    auto *TableGuardFnTy = llvm::FunctionType::get(
        getVoidTy(),
        {/* __sable_table_t *table */ getTablePtrTy(),
         /* std::uint32_t   index  */ getI32Ty()},
        false);
    llvm::Function::Create(
        /* Type    */ TableGuardFnTy,
        /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
        /* Name    */ "__sable_table_guard",
        /* Parent  */ Target);
  }

  auto *I32PtrTy = llvm::PointerType::getUnqual(getI32Ty());
  auto *TableSetFnTy = llvm::FunctionType::get(
      getVoidTy(),
      {/* __sable_table_t    *table    */ getTablePtrTy(),
       /* __sable_instance_t *instance */ getInstancePtrTy(),
       /* std::uint32        offset    */ getI32Ty(),
       /* std::uint32        count     */ getI32Ty(),
       /* std::uint32        *indices  */ I32PtrTy},
      false);
  llvm::Function::Create(
      /* Type    */ TableSetFnTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_table_set",
      /* Parent  */ Target);

  auto *TableCheckFnTy = llvm::FunctionType::get(
      getVoidTy(),
      {/* __sable_table_t *table            */ getTablePtrTy(),
       /* std::uint32_t   index             */ getI32Ty(),
       /* char const      *expect_signature */ getCStringPtrTy()},
      false);
  llvm::Function::Create(
      /* Type    */ TableCheckFnTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_table_check",
      /* Parent  */ Target);

  auto *TableFunctionFnTy = llvm::FunctionType::get(
      /* __sable_function_t *function_ptr */ getFunctionPtrTy(),
      {/* __sable_table_t   *table        */ getTablePtrTy(),
       /* std::uint32_t     index         */ getI32Ty()},
      false);
  llvm::Function::Create(
      /* Type    */ TableFunctionFnTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_table_function",
      /* Parent  */ Target);

  auto *TableContextFnTy = llvm::FunctionType::get(
      /* __sable_instance_t *instance */ getInstancePtrTy(),
      {/* __sable_table_t   *table    */ getTablePtrTy(),
       /* std::uint32_t     index     */ getI32Ty()},
      false);
  llvm::Function::Create(
      /* Type    */ TableContextFnTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_table_context",
      /* Parent  */ Target);

  auto *UnreachableFnTy = llvm::FunctionType::get(getVoidTy(), {}, false);
  llvm::Function::Create(
      /* Type    */ UnreachableFnTy,
      /* Linkage */ llvm::GlobalValue::LinkageTypes::ExternalLinkage,
      /* Name    */ "__sable_unreachable",
      /* Parent  */ Target);
}

EntityLayout::EntityLayout(
    mir::Module const &Source_, llvm::Module &Target_,
    TranslationOptions Options_)
    : Source(Source_), Target(Target_), Options(Options_) {
  setupInstanceType();
  setupBuiltins();
  setupFunctions();
  setupDataSegments();
  setupElementSegments();
  setupMemoryMetadata();
  setupTableMetadata();
  setupGlobalMetadata();
  setupFunctionMetadata();
  setupInitializer();
}

llvm::Type *EntityLayout::convertType(bytecode::ValueType const &Type) const {
  switch (Type.getKind()) {
  case bytecode::ValueTypeKind::I32: return getI32Ty();
  case bytecode::ValueTypeKind::I64: return getI64Ty();
  case bytecode::ValueTypeKind::F32: return getF32Ty();
  case bytecode::ValueTypeKind::F64: return getF64Ty();
  default: utility::unreachable();
  }
}

llvm::FunctionType *
EntityLayout::convertType(bytecode::FunctionType const &Type) const {
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

TranslationOptions const &EntityLayout::getTranslationOptions() const {
  return Options;
}

std::size_t EntityLayout::getOffset(mir::ASTNode const &Node) const {
  auto SearchIter = OffsetMap.find(std::addressof(Node));
  assert(SearchIter != OffsetMap.end());
  return std::get<1>(*SearchIter) + INSTANCE_ENTITY_START_OFFSET;
}

llvm::Constant *EntityLayout::operator[](mir::Data const &DataSegment) const {
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

llvm::Constant *
EntityLayout::operator[](mir::Element const &ElementSegment) const {
  auto SearchIter = ElementMap.find(std::addressof(ElementSegment));
  assert(SearchIter != ElementMap.end());
  return std::get<1>(*SearchIter);
}

llvm::Function *EntityLayout::getBuiltin(std::string_view Name) const {
  auto *Builtin = Target.getFunction(Name);
  assert(Builtin != nullptr);
  return Builtin;
}

llvm::Value *EntityLayout::get(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::Global const &Global) const {
  auto Offset = getOffset(Global);
  auto GlobalValueType = Global.getType().getType();
  auto *CastedToTy = llvm::PointerType::getUnqual(convertType(GlobalValueType));
  llvm::Value *GlobalInstance = Builder.CreateStructGEP(InstancePtr, Offset);
  GlobalInstance = Builder.CreateLoad(GlobalInstance);
  GlobalInstance = Builder.CreatePointerCast(GlobalInstance, CastedToTy);
  if (Global.hasName())
    GlobalInstance->setName(llvm::StringRef(Global.getName()));
  return GlobalInstance;
}

llvm::Value *EntityLayout::getContextPtr(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::Function const &Function) const {
  auto Offset = getOffset(Function);
  auto *ContextPtrAddr = Builder.CreateStructGEP(InstancePtr, Offset);
  return Builder.CreateLoad(ContextPtrAddr);
}

llvm::Value *EntityLayout::getFunctionPtr(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::Function const &Function) const {
  auto Offset = getOffset(Function) + 1;
  auto *FunctionTy = convertType(Function.getType());
  auto *FunctionPtrTy = llvm::PointerType::getUnqual(FunctionTy);
  llvm::Value *FunctionPtr = Builder.CreateStructGEP(InstancePtr, Offset);
  FunctionPtr = Builder.CreateLoad(FunctionPtr);
  FunctionPtr = Builder.CreatePointerCast(FunctionPtr, FunctionPtrTy);
  if (Function.hasName())
    FunctionPtr->setName(llvm::StringRef(Function.getName()));
  return FunctionPtr;
}

llvm::Value *EntityLayout::get(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    mir::Memory const &MMemory) const {
  auto Offset = getOffset(MMemory);
  llvm::Value *MemoryPtr = Builder.CreateStructGEP(InstancePtr, Offset);
  MemoryPtr = Builder.CreateLoad(MemoryPtr);
  if (MMemory.hasName()) MemoryPtr->setName(llvm::StringRef(MMemory.getName()));
  return MemoryPtr;
}

llvm::Value *EntityLayout::get(
    llvm::IRBuilder<> &Builder, llvm::Value *InstancePtr,
    const mir::Table &MTable) const {
  auto Offset = getOffset(MTable);
  llvm::Value *TablePtr = Builder.CreateStructGEP(InstancePtr, Offset);
  TablePtr = Builder.CreateLoad(TablePtr);
  if (MTable.hasName()) TablePtr->setName(llvm::StringRef(MTable.getName()));
  return TablePtr;
}

char EntityLayout::getSignature(bytecode::ValueType const &Type) const {
  switch (Type.getKind()) {
  case bytecode::ValueTypeKind::I32: return 'I';
  case bytecode::ValueTypeKind::I64: return 'J';
  case bytecode::ValueTypeKind::F32: return 'F';
  case bytecode::ValueTypeKind::F64: return 'D';
  default: utility::unreachable();
  }
}

char EntityLayout::getSignature(bytecode::GlobalType const &Type) const {
  auto const &ValueType = Type.getType();
  switch (Type.getMutability()) {
  case bytecode::MutabilityKind::Const:
    return static_cast<char>(std::toupper(getSignature(ValueType)));
  case bytecode::MutabilityKind::Var:
    return static_cast<char>(std::tolower(getSignature(ValueType)));
  default: utility::unreachable();
  }
}

std::string
EntityLayout::getSignature(bytecode::FunctionType const &Type) const {
  std::string Result;
  Result.reserve(Type.getNumParameter() + Type.getNumResult() + 1);
  for (auto const &ValueType : Type.getParamTypes())
    Result.push_back(getSignature(ValueType));
  Result.push_back(':');
  for (auto const &ValueType : Type.getResultTypes())
    Result.push_back(getSignature(ValueType));
  return Result;
}

llvm::Type *EntityLayout::getVoidTy() const {
  return llvm::Type::getVoidTy(Target.getContext());
}

llvm::PointerType *EntityLayout::getVoidPtrTy() const {
  return llvm::Type::getInt8PtrTy(Target.getContext());
}

llvm::PointerType *EntityLayout::getCStringPtrTy() const {
  return llvm::Type::getInt8PtrTy(Target.getContext());
}

llvm::Constant *EntityLayout::getCStringPtr(
    std::string_view Content, std::string_view Name) const {
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

llvm::IntegerType *EntityLayout::getI32Ty() const {
  return llvm::Type::getInt32Ty(Target.getContext());
}

llvm::ConstantInt *EntityLayout::getI32Constant(std::int32_t Value) const {
  return llvm::ConstantInt::get(getI32Ty(), Value);
}

llvm::IntegerType *EntityLayout::getI64Ty() const {
  return llvm::Type::getInt64Ty(Target.getContext());
}

llvm::ConstantInt *EntityLayout::getI64Constant(std::int64_t Value) const {
  return llvm::ConstantInt::get(getI64Ty(), Value);
}

llvm::Type *EntityLayout::getF32Ty() const {
  return llvm::Type::getFloatTy(Target.getContext());
}

llvm::ConstantFP *EntityLayout::getF32Constant(float Value) const {
  auto *Result = llvm::ConstantFP::get(getF32Ty(), Value);
  return llvm::dyn_cast<llvm::ConstantFP>(Result);
}

llvm::Type *EntityLayout::getF64Ty() const {
  return llvm::Type::getDoubleTy(Target.getContext());
}

llvm::ConstantFP *EntityLayout::getF64Constant(double Value) const {
  auto *Result = llvm::ConstantFP::get(getF64Ty(), Value);
  return llvm::dyn_cast<llvm::ConstantFP>(Result);
}

llvm::Type *EntityLayout::getPtrIntTy() const {
  auto &TargetDataLayout = Target.getDataLayout();
  return TargetDataLayout.getIntPtrType(Target.getContext());
}

llvm::PointerType *EntityLayout::getInstancePtrTy() const {
  auto *InstanceTy = getNamedStructTy("__sable_instance_t");
  return llvm::PointerType::getUnqual(InstanceTy);
}

llvm::StructType *EntityLayout::getMemoryMetadataTy() const {
  return getNamedStructTy("__sable_memory_metadata_t");
}

llvm::StructType *EntityLayout::getTableMetadataTy() const {
  return getNamedStructTy("__sable_table_metadata_t");
}

llvm::StructType *EntityLayout::getGlobalMetadataTy() const {
  return getNamedStructTy("__sable_global_metadata_t");
}

llvm::StructType *EntityLayout::getFunctionMetadataTy() const {
  return getNamedStructTy("__sable_function_metadata_t");
}

llvm::PointerType *EntityLayout::getMemoryPtrTy() const {
  auto *OpaqueTy = getOpaqueTy("__sable_memory_t");
  return llvm::PointerType::getUnqual(OpaqueTy);
}

llvm::PointerType *EntityLayout::getTablePtrTy() const {
  auto *OpaqueTy = getOpaqueTy("__sable_table_t");
  return llvm::PointerType::getUnqual(OpaqueTy);
}

llvm::PointerType *EntityLayout::getGlobalPtrTy() const {
  auto *OpaqueTy = getOpaqueTy("__sable_global_t");
  return llvm::PointerType::getUnqual(OpaqueTy);
}

llvm::PointerType *EntityLayout::getFunctionPtrTy() const {
  auto *OpaqueTy = getOpaqueTy("__sable_function_t");
  return llvm::PointerType::getUnqual(OpaqueTy);
}

llvm::GlobalVariable *EntityLayout::getMemoryMetadata() const {
  return Target.getGlobalVariable("__sable_memory_metadata");
}

llvm::GlobalVariable *EntityLayout::getTableMetadata() const {
  return Target.getGlobalVariable("__sable_table_metadata");
}

llvm::GlobalVariable *EntityLayout::getGlobalMetadata() const {
  return Target.getGlobalVariable("__sable_global_metadata");
}

llvm::GlobalVariable *EntityLayout::getFunctionMetadata() const {
  return Target.getGlobalVariable("__sable_function_metadata");
}
} // namespace codegen::llvm_instance
