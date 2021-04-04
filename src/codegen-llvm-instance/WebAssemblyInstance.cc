#include "WebAssemblyInstance.h"

#include <dlfcn.h>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/subrange.hpp>

#include <stdexcept>

void __sable_unreachable() { throw runtime::exceptions::Unreachable(); }

namespace runtime {
namespace {
bytecode::ValueType fromTypeChar(char TypeChar) {
  switch (std::toupper(TypeChar)) {
  case 'I': return bytecode::valuetypes::I32;
  case 'J': return bytecode::valuetypes::I64;
  case 'F': return bytecode::valuetypes::F32;
  case 'D': return bytecode::valuetypes::F64;
  default: utility::unreachable();
  }
}

char toTypeChar(bytecode::ValueType const &ValueType) {
  switch (ValueType.getKind()) {
  case bytecode::ValueTypeKind::I32: return 'I';
  case bytecode::ValueTypeKind::I64: return 'J';
  case bytecode::ValueTypeKind::F32: return 'F';
  case bytecode::ValueTypeKind::F64: return 'D';
  default: utility::unreachable();
  }
}
} // namespace

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
  struct MemoryType {
    std::uint32_t Min;
    std::uint32_t Max;
  };
  std::uint32_t Size, ISize, ESize;
  MemoryType const *Entities;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
};

struct WebAssemblyInstance::TableMetadata {
  struct TableType {
    std::uint32_t Min;
    std::uint32_t Max;
  };
  std::uint32_t Size, ISize, ESize;
  TableType const *Entities;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
};

struct WebAssemblyInstance::GlobalMetadata {
  std::uint32_t Size, ISize, ESize;
  char const *Entities;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
};

struct WebAssemblyInstance::FunctionMetadata {
  std::uint32_t Size, ISize, ESize;
  char const *const *Entities;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
};

WebAssemblyInstanceBuilder::WebAssemblyInstanceBuilder(char const *Path) {
  Instance = std::unique_ptr<WebAssemblyInstance>(new WebAssemblyInstance());
  // dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
  Instance->DLHandler = dlopen(Path, RTLD_NOW | RTLD_LOCAL);
  if (Instance->DLHandler == nullptr) fmt::print("{}\n", dlerror());
  assert(Instance->DLHandler != nullptr);
  // clang-format off
  auto *MemoryMetadata = reinterpret_cast
    <WebAssemblyInstance::MemoryMetadata *>
    (dlsym(Instance->DLHandler, "__sable_memory_metadata"));
  auto *TableMetadata = reinterpret_cast
    <WebAssemblyInstance::TableMetadata *>
    (dlsym(Instance->DLHandler, "__sable_table_metadata"));
  auto *GlobalMetadata = reinterpret_cast
    <WebAssemblyInstance::GlobalMetadata *>
    (dlsym(Instance->DLHandler, "__sable_global_metadata"));
  auto *FunctionMetadata = reinterpret_cast
    <WebAssemblyInstance::FunctionMetadata *>
    (dlsym(Instance->DLHandler, "__sable_function_metadata"));
  // clang-format on
  assert(MemoryMetadata != nullptr);
  assert(TableMetadata != nullptr);
  assert(GlobalMetadata != nullptr);
  assert(FunctionMetadata != nullptr);

  auto MemoryOffset = 4;
  auto TableOffset = MemoryOffset + MemoryMetadata->Size;
  auto GlobalOffset = TableOffset + TableMetadata->Size;
  auto FunctionOffset = GlobalOffset + GlobalMetadata->Size;
  auto Size = FunctionOffset + FunctionMetadata->Size;

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
    auto Min = MemoryMetadata.Entities[Index].Min;
    auto Max = MemoryMetadata.Entities[Index].Max;
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
    WebAssemblyGlobal &Global) {
  auto const &GlobalMetadata = Instance->getGlobalMetadata();
  for (std::size_t I = 0; I < GlobalMetadata.ISize; ++I) {
    if ((GlobalMetadata.Imports[I].ModuleName != ModuleName) ||
        (GlobalMetadata.Imports[I].EntityName != EntityName))
      continue;
    auto Index = GlobalMetadata.Imports[I].Index;
    auto ExpectTypeChar = std::toupper(GlobalMetadata.Entities[Index]);
    auto ActualTypeChar = toTypeChar(Global.getValueType());
    if (ExpectTypeChar != ActualTypeChar) continue;
    auto *InstancePtr = Global.asInstancePtr();
    Instance->getGlobal(Index) = InstancePtr;
    return true;
  }
  return false;
}

bool WebAssemblyInstanceBuilder::tryImport(
    std::string_view ModuleName, std::string_view EntityName,
    std::string_view TypeString, std::intptr_t Function) {
  auto const &FunctionMetadata = Instance->getFunctionMetadata();
  for (std::size_t I = 0; I < FunctionMetadata.ISize; ++I) {
    if ((FunctionMetadata.Imports[I].ModuleName != ModuleName) ||
        (FunctionMetadata.Imports[I].EntityName != EntityName))
      continue;
    auto Index = FunctionMetadata.Imports[I].Index;
    if (FunctionMetadata.Entities[Index] != TypeString) continue;
    auto *CastedPtr = reinterpret_cast<__sable_function_t *>(Function);
    Instance->getFunction(Index) = CastedPtr;
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

std::unique_ptr<WebAssemblyInstance> WebAssemblyInstanceBuilder::Build() {
  auto MemoryDefFirst = Instance->getMemoryMetadata().ISize;
  auto MemoryDefLast = Instance->getMemoryMetadata().Size;
  for (std::size_t I = MemoryDefFirst; I < MemoryDefLast; ++I) {
    auto Min = Instance->getMemoryMetadata().Entities[I].Min;
    auto Max = Instance->getMemoryMetadata().Entities[I].Max;
    auto *Memory = new WebAssemblyMemory(Min, Max);
    Memory->addUseSite(*Instance);
    Instance->getMemory(I) = Memory->asInstancePtr();
  }

  auto GlobalDefFirst = Instance->getGlobalMetadata().ISize;
  auto GlobalDefEnd = Instance->getGlobalMetadata().Size;
  for (std::size_t I = GlobalDefFirst; I < GlobalDefEnd; ++I) {
    auto TypeChar = Instance->getGlobalMetadata().Entities[I];
    auto GlobalValueType = fromTypeChar(TypeChar);
    auto *Global = new WebAssemblyGlobal(GlobalValueType);
    Instance->getGlobal(I) = Global->asInstancePtr();
  }

  using SableInitializeFnTy = void (*)(void *);
  // clang-format off
  auto *Initializer = reinterpret_cast<SableInitializeFnTy>
    (dlsym(Instance->DLHandler, "__sable_initialize"));
  // clang-format on
  assert(Initializer != nullptr);
  Initializer(Instance->Storage);

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
    auto *InstancePtr = Instance->getFunction(Index);
    auto *ExpectTypeStr = FunctionMetadata.Entities[Index];
    auto Entry = std::make_pair(InstancePtr, ExpectTypeStr);
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
  return reinterpret_cast<__sable_memory_t *&>(Storage[4 + Index]);
}

__sable_table_t *&WebAssemblyInstance::getTable(std::size_t Index) {
  assert(Index < getTableMetadata().Size);
  auto Offset = 4 + getMemoryMetadata().Size;
  return reinterpret_cast<__sable_table_t *&>(Storage[Offset + Index]);
}

__sable_global_t *&WebAssemblyInstance::getGlobal(std::size_t Index) {
  assert(Index < getGlobalMetadata().Size);
  auto Offset = 4 + getMemoryMetadata().Size + getTableMetadata().Size;
  return reinterpret_cast<__sable_global_t *&>(Storage[Offset + Index]);
}

__sable_function_t *&WebAssemblyInstance::getFunction(std::size_t Index) {
  assert(Index < getFunctionMetadata().Size);
  auto Offset = 4 + getMemoryMetadata().Size + getTableMetadata().Size +
                getGlobalMetadata().Size;
  return reinterpret_cast<__sable_function_t *&>(Storage[Offset + Index]);
}

void WebAssemblyInstance::replace(
    __sable_memory_t *Old, __sable_memory_t *New) {
  for (std::size_t I = 0; I < getMemoryMetadata().Size; ++I)
    if (getMemory(I) == Old) {
      getMemory(I) = New;
      return;
    }
  utility::unreachable();
}

WebAssemblyInstance::~WebAssemblyInstance() noexcept {
  if (Storage != nullptr) {
    for (std::size_t I = 0; I < getMemoryMetadata().Size; ++I) {
      auto *MemoryPtr = getMemory(I);
      auto *Memory = WebAssemblyMemory::fromInstancePtr(MemoryPtr);
      Memory->removeUseSite(*this);
    }

    auto MemoryDefFirst = getMemoryMetadata().ISize;
    auto MemoryDefLast = getMemoryMetadata().Size;
    for (std::size_t I = MemoryDefFirst; I < MemoryDefLast; ++I) {
      auto *MemoryPtr = getMemory(I);
      auto *Memory = WebAssemblyMemory::fromInstancePtr(MemoryPtr);
      if (Memory != nullptr) delete Memory;
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
  if (DLHandler != nullptr) dlclose(DLHandler);
}

WebAssemblyMemory *WebAssemblyInstance::getMemory(std::string_view Name) {
  auto SearchIter = ExportedMemories.find(Name);
  if (SearchIter == ExportedMemories.end()) return nullptr;
  return WebAssemblyMemory::fromInstancePtr(std::get<1>(*SearchIter));
}

WebAssemblyGlobal *WebAssemblyInstance::getGlobal(std::string_view Name) {
  auto SearchIter = ExportedGlobals.find(Name);
  if (SearchIter == ExportedGlobals.end()) return nullptr;
  return WebAssemblyGlobal::fromInstancePtr(std::get<1>(*SearchIter));
}

std::optional<WebAssemblyCallee>
WebAssemblyInstance::getFunction(std::string_view Name) {
  auto SearchIter = ExportedFunctions.find(Name);
  if (SearchIter == ExportedFunctions.end()) return std::nullopt;
  auto *ExpectTypeStr = std::get<1>(std::get<1>(*SearchIter));
  auto *CalleePtr = std::get<0>(std::get<1>(*SearchIter));
  return WebAssemblyCallee(asInstancePtr(), ExpectTypeStr, CalleePtr);
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