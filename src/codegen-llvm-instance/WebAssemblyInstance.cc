#include "WebAssemblyInstance.h"

#include <dlfcn.h>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/subrange.hpp>

#include <stdexcept>

#define INSTANCE_ENTITY_START_OFFSET 4

void __sable_unreachable() { throw runtime::exceptions::Unreachable(); }

namespace runtime {
namespace detail {
template <>
bytecode::ValueType fromSignature<bytecode::ValueType>(char Signature) {
  switch (std::toupper(Signature)) {
  case 'I': return bytecode::valuetypes::I32;
  case 'J': return bytecode::valuetypes::I64;
  case 'F': return bytecode::valuetypes::F32;
  case 'D': return bytecode::valuetypes::F64;
  default: utility::unreachable();
  }
}

template <>
bytecode::GlobalType fromSignature<bytecode::GlobalType>(char Signature) {
  auto Mutability = (std::isupper(Signature)) ? bytecode::MutabilityKind::Const
                                              : bytecode::MutabilityKind::Var;
  return bytecode::GlobalType(
      Mutability, fromSignature<bytecode::ValueType>(Signature));
}

template <>
bytecode::FunctionType
fromSignature<bytecode::FunctionType>(std::string_view FuncSignature) {
  std::vector<bytecode::ValueType> ParamTypes;
  std::vector<bytecode::ValueType> ResultTypes;
  bool SeenSeparator = false;
  for (auto Signature : FuncSignature) {
    if (Signature == ':') {
      assert(!SeenSeparator);
      SeenSeparator = true;
      continue;
    }
    auto ValueType = fromSignature<bytecode::ValueType>(Signature);
    if (!SeenSeparator) {
      ParamTypes.push_back(ValueType);
    } else {
      ResultTypes.push_back(ValueType);
    }
  }
  return bytecode::FunctionType(std::move(ParamTypes), std::move(ResultTypes));
}

char toSignature(bytecode::ValueType const &Type) {
  switch (Type.getKind()) {
  case bytecode::ValueTypeKind::I32: return 'I';
  case bytecode::ValueTypeKind::I64: return 'J';
  case bytecode::ValueTypeKind::F32: return 'F';
  case bytecode::ValueTypeKind::F64: return 'D';
  default: utility::unreachable();
  }
}

char toSignature(bytecode::GlobalType const &Type) {
  auto Signature = toSignature(Type.getType());
  switch (Type.getMutability()) {
  case bytecode::MutabilityKind::Const:
    return static_cast<char>(std::toupper(Signature));
  case bytecode::MutabilityKind::Var:
    return static_cast<char>(std::tolower(Signature));
  default: utility::unreachable();
  }
}

std::string toSignature(bytecode::FunctionType const &Type) {
  std::string Result;
  Result.reserve(Type.getNumParameter() + Type.getNumResult() + 1);
  for (auto const &ValueType : Type.getParamTypes())
    Result.push_back(toSignature(ValueType));
  Result.push_back(':');
  for (auto const &ValueType : Type.getResultTypes())
    Result.push_back(toSignature(ValueType));
  return Result;
}
} // namespace detail

struct WebAssemblyInstance::ImportDescriptor {
  std::uint32_t Index;
  char const *ModuleName;
  char const *EntityName;
};

struct WebAssemblyInstance::ExportDescriptor {
  std::uint32_t Index;
  char const *Name;
};

struct WebAssemblyInstance::MemoryMetadata {
  struct MemorySignature {
    std::uint32_t Min;
    std::uint32_t Max;
  };
  std::uint32_t Size, ISize, ESize;
  MemorySignature const *Signatures;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
};

struct WebAssemblyInstance::TableMetadata {
  struct TableSignature {
    std::uint32_t Min;
    std::uint32_t Max;
  };
  std::uint32_t Size, ISize, ESize;
  TableSignature const *Signatures;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
};

struct WebAssemblyInstance::GlobalMetadata {
  std::uint32_t Size, ISize, ESize;
  char const *Signatures;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
};

struct WebAssemblyInstance::FunctionMetadata {
  std::uint32_t Size, ISize, ESize;
  char const *const *Signatures;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
};

WebAssemblyInstanceBuilder::WebAssemblyInstanceBuilder(
    std::filesystem::path const &Path) {
  Instance = std::unique_ptr<WebAssemblyInstance>(new WebAssemblyInstance());
  auto AbsolutPath = std::filesystem::absolute(Path);
  Instance->DLHandler = dlopen(AbsolutPath.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (Instance->DLHandler == nullptr)
    throw exceptions::MalformedInstanceLibrary(dlerror());
  // clang-format off
  auto *MemoryMetadata = reinterpret_cast
    <WebAssemblyInstance::MemoryMetadata *>
    (dlsym(Instance->DLHandler, "__sable_memory_metadata"));
  if (MemoryMetadata == nullptr)
    throw exceptions::MalformedInstanceLibrary(dlerror());
  auto *TableMetadata = reinterpret_cast
    <WebAssemblyInstance::TableMetadata *>
    (dlsym(Instance->DLHandler, "__sable_table_metadata"));
  if (TableMetadata == nullptr)
    throw exceptions::MalformedInstanceLibrary(dlerror());
  auto *GlobalMetadata = reinterpret_cast
    <WebAssemblyInstance::GlobalMetadata *>
    (dlsym(Instance->DLHandler, "__sable_global_metadata"));
  if (GlobalMetadata == nullptr)
    throw exceptions::MalformedInstanceLibrary(dlerror());
  auto *FunctionMetadata = reinterpret_cast
    <WebAssemblyInstance::FunctionMetadata *>
    (dlsym(Instance->DLHandler, "__sable_function_metadata"));
  if (FunctionMetadata == nullptr)
    throw exceptions::MalformedInstanceLibrary(dlerror());
  // clang-format on

  auto MemoryOffset = INSTANCE_ENTITY_START_OFFSET;
  auto TableOffset = MemoryOffset + MemoryMetadata->Size;
  auto GlobalOffset = TableOffset + TableMetadata->Size;
  auto FunctionOffset = GlobalOffset + GlobalMetadata->Size;
  auto Size = FunctionOffset + FunctionMetadata->Size * 2;

  Instance->Storage = new void *[Size + 1];
  std::fill(Instance->Storage, Instance->Storage + Size + 1, nullptr);
  Instance->Storage[0] = Instance.get();
  Instance->Storage = std::addressof(Instance->Storage[1]);

  Instance->Storage[0] = MemoryMetadata;
  Instance->Storage[1] = TableMetadata;
  Instance->Storage[2] = GlobalMetadata;
  Instance->Storage[3] = FunctionMetadata;
}

bool WebAssemblyInstanceBuilder::tryImport(
    std::string_view ModuleName, std::string_view EntityName,
    WebAssemblyMemory &Memory) {
  auto const &MemoryMetadata = Instance->getMemoryMetadata();
  for (std::size_t I = 0; I < MemoryMetadata.ISize; ++I) {
    if ((MemoryMetadata.Imports[I].ModuleName != ModuleName) ||
        (MemoryMetadata.Imports[I].EntityName != EntityName))
      continue;
    auto Index = MemoryMetadata.Imports[I].Index;
    auto Min = MemoryMetadata.Signatures[Index].Min;
    auto Max = MemoryMetadata.Signatures[Index].Max;
    if (!(Memory.getSize() >= Min)) continue;
    if (!(Memory.getMaxSize() <= Max)) continue;
    Memory.addUseSite(*Instance);
    auto *InstancePtr = Memory.asInstancePtr();
    Instance->getMemory(Index) = InstancePtr;
    return true;
  }
  return false;
}

bool WebAssemblyInstanceBuilder::tryImport(
    std::string_view ModuleName, std::string_view EntityName,
    WebAssemblyTable &Table) {
  auto const &TableMetadata = Instance->getTableMetadata();
  for (std::size_t I = 0; I < TableMetadata.ISize; ++I) {
    if ((TableMetadata.Imports[I].ModuleName != ModuleName) ||
        (TableMetadata.Imports[I].EntityName != EntityName))
      continue;
    auto Index = TableMetadata.Imports[I].Index;
    auto Min = TableMetadata.Signatures[Index].Min;
    auto Max = TableMetadata.Signatures[Index].Min;
    if (!(Table.getSize() >= Min)) continue;
    if (!(Table.getMaxSize() <= Max)) continue;
    auto *InstancePtr = Table.asInstancePtr();
    Instance->getTable(Index) = InstancePtr;
    return true;
  }
  return false;
}

bool WebAssemblyInstanceBuilder::tryImport(
    std::string_view ModuleName, std::string_view EntityName,
    WebAssemblyGlobal &Global) {
  auto const &GlobalMetadata = Instance->getGlobalMetadata();
  for (std::size_t I = 0; I < GlobalMetadata.ISize; ++I) {
    if ((GlobalMetadata.Imports[I].ModuleName != ModuleName) ||
        (GlobalMetadata.Imports[I].EntityName != EntityName))
      continue;
    auto Index = GlobalMetadata.Imports[I].Index;
    auto ExpectTypeChar = std::toupper(GlobalMetadata.Signatures[Index]);
    auto ActualTypeChar = detail::toSignature(Global.getValueType());
    if (ExpectTypeChar != ActualTypeChar) continue;
    auto *InstancePtr = Global.asInstancePtr();
    Instance->getGlobal(Index) = InstancePtr;
    return true;
  }
  return false;
}

bool WebAssemblyInstanceBuilder::tryImport(
    std::string_view ModuleName, std::string_view EntityName,
    WebAssemblyCallee Callee) {
  auto const &FunctionMetadata = Instance->getFunctionMetadata();
  for (std::size_t I = 0; I < FunctionMetadata.ISize; ++I) {
    if ((FunctionMetadata.Imports[I].ModuleName != ModuleName) ||
        (FunctionMetadata.Imports[I].EntityName != EntityName))
      continue;
    auto Index = FunctionMetadata.Imports[I].Index;
    if (FunctionMetadata.Signatures[Index] != Callee.getSignature()) continue;
    Instance->getContextPtr(Index) = Callee.getContextPtr();
    Instance->getFunctionPtr(Index) = Callee.getFunctionPtr();
    return true;
  }
  return false;
}

bool WebAssemblyInstanceBuilder::tryImport(
    std::string_view ModuleName, std::string_view EntityName,
    std::string_view Signature, std::intptr_t Function) {
  auto const &FunctionMetadata = Instance->getFunctionMetadata();
  for (std::size_t I = 0; I < FunctionMetadata.ISize; ++I) {
    if ((FunctionMetadata.Imports[I].ModuleName != ModuleName) ||
        (FunctionMetadata.Imports[I].EntityName != EntityName))
      continue;
    auto Index = FunctionMetadata.Imports[I].Index;
    if (FunctionMetadata.Signatures[Index] != Signature) continue;
    auto *CastedPtr = reinterpret_cast<__sable_function_t *>(Function);
    Instance->getContextPtr(Index) = nullptr;
    Instance->getFunctionPtr(Index) = CastedPtr;
    return true;
  }
  return false;
}

WebAssemblyInstanceBuilder &WebAssemblyInstanceBuilder::import(
    std::string_view ModuleName, std::string_view EntityName,
    WebAssemblyMemory &Memory) {
  if (!tryImport(ModuleName, EntityName, Memory))
    throw std::invalid_argument("cannot locate import memory");
  return *this;
}

WebAssemblyInstanceBuilder &WebAssemblyInstanceBuilder::import(
    std::string_view ModuleName, std::string_view EntityName,
    WebAssemblyTable &Table) {
  if (!tryImport(ModuleName, EntityName, Table))
    throw std::invalid_argument("cannot locate import table");
  return *this;
}

WebAssemblyInstanceBuilder &WebAssemblyInstanceBuilder::import(
    std::string_view ModuleName, std::string_view EntityName,
    WebAssemblyGlobal &Global) {
  if (!tryImport(ModuleName, EntityName, Global))
    throw std::invalid_argument("cannot locate import global");
  return *this;
}

WebAssemblyInstanceBuilder &WebAssemblyInstanceBuilder::import(
    std::string_view ModuleName, std::string_view EntityName,
    std::string_view TypeString, std::intptr_t Function) {
  if (!tryImport(ModuleName, EntityName, TypeString, Function))
    throw std::invalid_argument("cannot locate import function");
  return *this;
}

WebAssemblyInstanceBuilder &WebAssemblyInstanceBuilder::import(
    std::string_view ModuleName, std::string_view EntityName,
    WebAssemblyCallee Callee) {
  if (!tryImport(ModuleName, EntityName, Callee))
    throw std::invalid_argument("cannot locate import function");
  return *this;
}

std::unique_ptr<WebAssemblyInstance> WebAssemblyInstanceBuilder::Build() {
  auto MemoryDefFirst = Instance->getMemoryMetadata().ISize;
  auto MemoryDefLast = Instance->getMemoryMetadata().Size;
  for (std::size_t I = MemoryDefFirst; I < MemoryDefLast; ++I) {
    auto Min = Instance->getMemoryMetadata().Signatures[I].Min;
    auto Max = Instance->getMemoryMetadata().Signatures[I].Max;
    auto *Memory = new WebAssemblyMemory(Min, Max);
    Memory->addUseSite(*Instance);
    Instance->getMemory(I) = Memory->asInstancePtr();
  }

  auto TableDefStart = Instance->getTableMetadata().ISize;
  auto TableDefLast = Instance->getTableMetadata().Size;
  for (std::size_t I = TableDefStart; I < TableDefLast; ++I) {
    auto Min = Instance->getTableMetadata().Signatures[I].Min;
    auto Max = Instance->getTableMetadata().Signatures[I].Max;
    auto *Table = new WebAssemblyTable(Min, Max);
    Instance->getTable(I) = Table->asInstancePtr();
  }

  auto GlobalDefFirst = Instance->getGlobalMetadata().ISize;
  auto GlobalDefEnd = Instance->getGlobalMetadata().Size;
  for (std::size_t I = GlobalDefFirst; I < GlobalDefEnd; ++I) {
    auto TypeChar = Instance->getGlobalMetadata().Signatures[I];
    auto GlobalValueType = detail::fromSignature<bytecode::ValueType>(TypeChar);
    auto *Global = new WebAssemblyGlobal(GlobalValueType);
    Instance->getGlobal(I) = Global->asInstancePtr();
  }

  using SableInitializeFnTy = void (*)(void *);
  // clang-format off
  auto *Initializer = reinterpret_cast<SableInitializeFnTy>
    (dlsym(Instance->DLHandler, "__sable_initialize"));
  if (Initializer == nullptr)
    throw exceptions::MalformedInstanceLibrary(dlerror());
  // clang-format on
  Initializer(Instance->Storage);

  for (std::size_t I = 0; I < Instance->getMemoryMetadata().Size; ++I)
    if (Instance->getMemory(I) == nullptr)
      throw std::runtime_error("incomplete instance (missing memory)");
  for (std::size_t I = 0; I < Instance->getTableMetadata().Size; ++I)
    if (Instance->getTable(I) == nullptr)
      throw std::runtime_error("incomplete instance (missing table)");
  for (std::size_t I = 0; I < Instance->getGlobalMetadata().Size; ++I)
    if (Instance->getGlobal(I) == nullptr)
      throw std::runtime_error("incomplete instance (missing global)");
  for (std::size_t I = 0; I < Instance->getFunctionMetadata().Size; ++I)
    if (Instance->getFunctionPtr(I) == nullptr)
      throw std::runtime_error("incomplete instance (missing function)");

  auto const &MemoryMetadata = Instance->getMemoryMetadata();
  auto const &TableMetadata = Instance->getTableMetadata();
  auto const &GlobalMetadata = Instance->getGlobalMetadata();
  auto const &FunctionMetadata = Instance->getFunctionMetadata();

  Instance->ExportedMemories.reserve(MemoryMetadata.ESize);
  for (std::size_t I = 0; I < MemoryMetadata.ESize; ++I) {
    std::string_view Name(MemoryMetadata.Exports[I].Name);
    auto *InstancePtr = Instance->getMemory(MemoryMetadata.Exports[I].Index);
    Instance->ExportedMemories.emplace(Name, InstancePtr);
  }

  Instance->ExportedTables.reserve(TableMetadata.ESize);
  for (std::size_t I = 0; I < TableMetadata.ESize; ++I) {
    std::string_view Name(TableMetadata.Exports[I].Name);
    auto *InstancePtr = Instance->getTable(TableMetadata.Exports[I].Index);
    Instance->ExportedTables.emplace(Name, InstancePtr);
  }

  Instance->ExportedGlobals.reserve(GlobalMetadata.ESize);
  for (std::size_t I = 0; I < GlobalMetadata.ESize; ++I) {
    std::string_view Name(GlobalMetadata.Exports[I].Name);
    auto *InstancePtr = Instance->getGlobal(GlobalMetadata.Exports[I].Index);
    Instance->ExportedGlobals.emplace(Name, InstancePtr);
  }

  Instance->ExportedFunctions.reserve(FunctionMetadata.ESize);
  for (std::size_t I = 0; I < FunctionMetadata.ESize; ++I) {
    auto Index = FunctionMetadata.Exports[I].Index;
    std::string_view Name(FunctionMetadata.Exports[I].Name);
    auto *ContextPtr = Instance->getContextPtr(Index);
    auto *FunctionPtr = Instance->getFunctionPtr(Index);
    auto *Signature = FunctionMetadata.Signatures[Index];
    WebAssemblyInstance::FunctionEntry Entry{
        .ContextPtr = ContextPtr,
        .FunctionPtr = FunctionPtr,
        .Signature = Signature};
    Instance->ExportedFunctions.emplace(Name, Entry);
  }

  return std::move(Instance);
}

WebAssemblyInstance::MemoryMetadata const &
WebAssemblyInstance::getMemoryMetadata() const {
  return *reinterpret_cast<MemoryMetadata *>(Storage[0]);
}

WebAssemblyInstance::TableMetadata const &
WebAssemblyInstance::getTableMetadata() const {
  return *reinterpret_cast<TableMetadata *>(Storage[1]);
}

WebAssemblyInstance::GlobalMetadata const &
WebAssemblyInstance::getGlobalMetadata() const {
  return *reinterpret_cast<GlobalMetadata *>(Storage[2]);
}

WebAssemblyInstance::FunctionMetadata const &
WebAssemblyInstance::getFunctionMetadata() const {
  return *reinterpret_cast<FunctionMetadata *>(Storage[3]);
}

__sable_memory_t *&WebAssemblyInstance::getMemory(std::size_t Index) {
  assert(Index < getMemoryMetadata().Size);
  auto Offset = INSTANCE_ENTITY_START_OFFSET + Index;
  return reinterpret_cast<__sable_memory_t *&>(Storage[Offset]);
}

__sable_table_t *&WebAssemblyInstance::getTable(std::size_t Index) {
  assert(Index < getTableMetadata().Size);
  auto Offset = INSTANCE_ENTITY_START_OFFSET + getMemoryMetadata().Size + Index;
  return reinterpret_cast<__sable_table_t *&>(Storage[Offset]);
}

__sable_global_t *&WebAssemblyInstance::getGlobal(std::size_t Index) {
  assert(Index < getGlobalMetadata().Size);
  auto Offset = INSTANCE_ENTITY_START_OFFSET + getMemoryMetadata().Size +
                getTableMetadata().Size + Index;
  return reinterpret_cast<__sable_global_t *&>(Storage[Offset]);
}

__sable_instance_t *&WebAssemblyInstance::getContextPtr(std::size_t Index) {
  assert(Index < getFunctionMetadata().Size);
  auto Offset = INSTANCE_ENTITY_START_OFFSET + getMemoryMetadata().Size +
                getTableMetadata().Size + getGlobalMetadata().Size;
  Offset = Offset + Index * 2;
  return reinterpret_cast<__sable_instance_t *&>(Storage[Offset]);
}

__sable_function_t *&WebAssemblyInstance::getFunctionPtr(std::size_t Index) {
  assert(Index < getFunctionMetadata().Size);
  auto Offset = INSTANCE_ENTITY_START_OFFSET + getMemoryMetadata().Size +
                getTableMetadata().Size + getGlobalMetadata().Size;
  Index = Offset + Index * 2 + 1;
  return reinterpret_cast<__sable_function_t *&>(Storage[Index]);
}

char const *WebAssemblyInstance::getSignature(std::size_t Index) const {
  assert(Index < getFunctionMetadata().Size);
  return getFunctionMetadata().Signatures[Index];
}

void WebAssemblyInstance::replace(
    __sable_memory_t *Old, __sable_memory_t *New) {
  auto HasReplaced = false;
  for (std::size_t I = 0; I < getMemoryMetadata().Size; ++I)
    if (getMemory(I) == Old) {
      getMemory(I) = New;
      HasReplaced = true;
      break;
    }
  for (auto const &[Name, MemoryPtr] : ExportedMemories) {
    if (MemoryPtr == Old) {
      ExportedMemories[Name] = New;
      HasReplaced = true;
      break;
    }
  }
  utility::ignore(HasReplaced);
  assert(HasReplaced);
}

WebAssemblyInstance::~WebAssemblyInstance() noexcept {
  auto *DLHandlerToFree = DLHandler;
  if (Storage != nullptr) {
    for (std::size_t I = 0; I < getMemoryMetadata().Size; ++I) {
      auto *MemoryPtr = getMemory(I);
      auto *Memory = WebAssemblyMemory::fromInstancePtr(MemoryPtr);
      if (Memory != nullptr) Memory->removeUseSite(*this);
    }

    auto MemoryDefFirst = getMemoryMetadata().ISize;
    auto MemoryDefLast = getMemoryMetadata().Size;
    for (std::size_t I = MemoryDefFirst; I < MemoryDefLast; ++I) {
      auto *MemoryPtr = getMemory(I);
      auto *Memory = WebAssemblyMemory::fromInstancePtr(MemoryPtr);
      if (Memory != nullptr) delete Memory;
    }

    auto TableDefFirst = getTableMetadata().ISize;
    auto TableDefLast = getTableMetadata().Size;
    for (std::size_t I = TableDefFirst; I < TableDefLast; ++I) {
      auto *TablePtr = getTable(I);
      auto *Table = WebAssemblyTable::fromInstancePtr(TablePtr);
      if (Table != nullptr) delete Table;
    }

    auto GlobalDefFirst = getGlobalMetadata().ISize;
    auto GlobalDefLast = getGlobalMetadata().Size;
    for (std::size_t I = GlobalDefFirst; I < GlobalDefLast; ++I) {
      auto *GlobalPtr = getGlobal(I);
      auto *Global = WebAssemblyGlobal::fromInstancePtr(GlobalPtr);
      if (Global != nullptr) delete Global;
    }
    delete[] std::addressof(Storage[-1]);
  }
  if (DLHandlerToFree != nullptr) dlclose(DLHandlerToFree);
}

WebAssemblyMemory &WebAssemblyInstance::getMemory(std::string_view Name) {
  auto *Memory = tryGetMemory(Name);
  if (Memory == nullptr)
    throw std::invalid_argument("cannot locate export memory");
  return *Memory;
}

WebAssemblyTable &WebAssemblyInstance::getTable(std::string_view Name) {
  auto *Table = tryGetTable(Name);
  if (Table == nullptr)
    throw std::invalid_argument("cannot locate export table");
  return *Table;
}

WebAssemblyGlobal &WebAssemblyInstance::getGlobal(std::string_view Name) {
  auto *Global = tryGetGlobal(Name);
  if (Global == nullptr)
    throw std::invalid_argument("cannot locate export global");
  return *Global;
}

WebAssemblyCallee WebAssemblyInstance::getFunction(std::string_view Name) {
  auto Function = tryGetFunction(Name);
  if (!Function.has_value())
    throw std::invalid_argument("cannot locate export function");
  return *Function;
}

WebAssemblyMemory *WebAssemblyInstance::tryGetMemory(std::string_view Name) {
  auto SearchIter = ExportedMemories.find(Name);
  if (SearchIter == ExportedMemories.end()) return nullptr;
  return WebAssemblyMemory::fromInstancePtr(std::get<1>(*SearchIter));
}

WebAssemblyTable *WebAssemblyInstance::tryGetTable(std::string_view Name) {
  auto SearchIter = ExportedTables.find(Name);
  if (SearchIter == ExportedTables.end()) return nullptr;
  return WebAssemblyTable::fromInstancePtr(std::get<1>(*SearchIter));
}

WebAssemblyGlobal *WebAssemblyInstance::tryGetGlobal(std::string_view Name) {
  auto SearchIter = ExportedGlobals.find(Name);
  if (SearchIter == ExportedGlobals.end()) return nullptr;
  return WebAssemblyGlobal::fromInstancePtr(std::get<1>(*SearchIter));
}

std::optional<WebAssemblyCallee>
WebAssemblyInstance::tryGetFunction(std::string_view Name) {
  auto SearchIter = ExportedFunctions.find(Name);
  if (SearchIter == ExportedFunctions.end()) return std::nullopt;
  auto *Signature = std::get<1>(*SearchIter).Signature;
  auto *FunctionPtr = std::get<1>(*SearchIter).FunctionPtr;
  auto *ContextPtr = std::get<1>(*SearchIter).ContextPtr;
  return WebAssemblyCallee(ContextPtr, FunctionPtr, Signature);
}

__sable_instance_t *WebAssemblyInstance::asInstancePtr() {
  return reinterpret_cast<__sable_instance_t *>(Storage);
}

WebAssemblyInstance *
WebAssemblyInstance::fromInstancePtr(__sable_instance_t *InstancePtr) {
  auto *CastedPtr = reinterpret_cast<void **>(InstancePtr);
  return reinterpret_cast<WebAssemblyInstance *>(CastedPtr[-1]);
}
} // namespace runtime