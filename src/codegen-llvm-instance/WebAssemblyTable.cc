#include "WebAssemblyInstance.h"

void __sable_table_guard(__sable_table_t *TablePtr, std::uint32_t Index) {
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  if (!(Index <= Table->getSize()))
    throw runtime::exceptions::TableAccessOutOfBound(*Table, Index);
}

void __sable_table_check(
    __sable_table_t *TablePtr, std::uint32_t Index, char const *TypeString) {
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  if (Table->isNull(Index))
    throw runtime::exceptions::BadTableEntry(*Table, Index);
  if (Table->getTypeString(Index) != TypeString) {
    auto ExpectType =
        runtime::detail::fromTypeString(Table->getTypeString(Index));
    auto ActualType = runtime::detail::fromTypeString(TypeString);
    throw runtime::exceptions::TableTypeMismatch(
        *Table, Index, std::move(ExpectType), std::move(ActualType));
  }
}

__sable_instance_t *
__sable_table_instance_closure(__sable_table_t *TablePtr, std::uint32_t Index) {
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  return Table->getInstanceClosure(Index);
}

__sable_function_t *
__sable_table_function_ptr(__sable_table_t *TablePtr, std::uint32_t Index) {
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  return Table->getFunctionPointer(Index);
}

void __sable_table_set(
    __sable_table_t *TablePtr, __sable_instance_t *InstancePtr,
    std::uint32_t StartPos, std::uint32_t Count, std::uint32_t Indices[]) {
  auto *Instance = runtime::WebAssemblyInstance::fromInstancePtr(InstancePtr);
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  for (std::uint32_t I = 0; I < Count; ++I) {
    auto Index = Indices[I];
    auto *InstanceClosure = Instance->getInstanceClosure(Index);
    auto *Function = Instance->getFunction(Index);
    auto *TypeString = Instance->getTypeString(Index);
    Table->set(StartPos + I, InstanceClosure, Function, TypeString);
  }
}

namespace runtime {
void WebAssemblyTable::set(
    std::uint32_t Index, __sable_instance_t *InstanceClosurePointer,
    __sable_function_t *FunctionPointer, std::string_view TypeString) {
  assert(Index < Storage.size());
  auto Entry = std::make_tuple(
      InstanceClosurePointer, FunctionPointer, std::string(TypeString));
  Storage[Index] = Entry;
}

__sable_instance_t *
WebAssemblyTable::getInstanceClosure(std::uint32_t Index) const {
  assert(Index < Storage.size());
  return std::get<0>(Storage[Index]);
}

__sable_function_t *
WebAssemblyTable::getFunctionPointer(std::uint32_t Index) const {
  assert(Index < Storage.size());
  return std::get<1>(Storage[Index]);
}

std::string_view WebAssemblyTable::getTypeString(std::uint32_t Index) const {
  assert(Index < Storage.size());
  return std::get<2>(Storage[Index]);
}

WebAssemblyTable::WebAssemblyTable(std::uint32_t NumEntries)
    : WebAssemblyTable(NumEntries, NO_MAXIMUM) {}

WebAssemblyTable::WebAssemblyTable(
    std::uint32_t NumEntries, std::uint32_t MaxNumEntries)
    : Size(NumEntries), MaxSize(MaxNumEntries), Storage() {
  Storage.resize(NumEntries, std::make_tuple(nullptr, nullptr, std::string()));
}

std::uint32_t WebAssemblyTable::getSize() const { return Size; }

bool WebAssemblyTable::hasMaxSize() const { return MaxSize != NO_MAXIMUM; }

std::uint32_t WebAssemblyTable::getMaxSize() const { return MaxSize; }

bool WebAssemblyTable::isNull(std::uint32_t Index) const {
  if (!(Index < getSize()))
    throw exceptions::TableAccessOutOfBound(*this, Index);
  return std::get<1>(Storage[Index]) == nullptr;
}

bytecode::FunctionType WebAssemblyTable::getType(std::uint32_t Index) const {
  if (isNull(Index)) throw exceptions::BadTableEntry(*this, Index);
  return detail::fromTypeString(std::get<2>(Storage[Index]));
}

WebAssemblyCallee WebAssemblyTable::get(std::uint32_t Index) const {
  if (isNull(Index)) throw exceptions::BadTableEntry(*this, Index);
  auto *InstanceClosure = std::get<0>(Storage[Index]);
  auto *FunctionPointer = std::get<1>(Storage[Index]);
  auto *TypeString = std::get<2>(Storage[Index]).data();
  return WebAssemblyCallee(InstanceClosure, FunctionPointer, TypeString);
}

void WebAssemblyTable::set(std::uint32_t Index, WebAssemblyCallee Callee) {
  auto *InstanceClosure = Callee.getInstanceClosure();
  auto *FunctionPointer = Callee.getFunction();
  auto *TypeString = Callee.getTypeString();
  set(Index, InstanceClosure, FunctionPointer, TypeString);
}

__sable_table_t *WebAssemblyTable::asInstancePtr() {
  return reinterpret_cast<__sable_table_t *>(this);
}

WebAssemblyTable *
WebAssemblyTable::fromInstancePtr(__sable_table_t *InstancePtr) {
  return reinterpret_cast<WebAssemblyTable *>(InstancePtr);
}

} // namespace runtime
