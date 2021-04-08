#include "WebAssemblyInstance.h"

void __sable_table_guard(__sable_table_t *TablePtr, std::uint32_t Index) {
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  if (!(Index <= Table->getSize()))
    throw runtime::exceptions::TableAccessOutOfBound(*Table, Index);
}

void __sable_table_check(
    __sable_table_t *TablePtr, std::uint32_t Index, char const *Signature) {
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  if (Table->isNull(Index))
    throw runtime::exceptions::BadTableEntry(*Table, Index);
  if (Table->getSignature(Index) != Signature) {
    auto ExpectType = runtime::detail::fromSignature<bytecode::FunctionType>(
        Table->getSignature(Index));
    auto ActualType =
        runtime::detail::fromSignature<bytecode::FunctionType>(Signature);
    throw runtime::exceptions::TableTypeMismatch(
        *Table, Index, std::move(ExpectType), std::move(ActualType));
  }
}

__sable_instance_t *
__sable_table_context(__sable_table_t *TablePtr, std::uint32_t Index) {
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  return Table->getContextPtr(Index);
}

__sable_function_t *
__sable_table_function(__sable_table_t *TablePtr, std::uint32_t Index) {
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  return Table->getFunctionPtr(Index);
}

void __sable_table_set(
    __sable_table_t *TablePtr, __sable_instance_t *InstancePtr,
    std::uint32_t Offset, std::uint32_t Count, std::uint32_t Indices[]) {
  auto *Instance = runtime::WebAssemblyInstance::fromInstancePtr(InstancePtr);
  auto *Table = runtime::WebAssemblyTable::fromInstancePtr(TablePtr);
  for (std::uint32_t I = 0; I < Count; ++I) {
    auto Index = Indices[I];
    auto *ContextPtr = Instance->getContextPtr(Index);
    auto *Function = Instance->getFunctionPtr(Index);
    auto *TypeString = Instance->getSignature(Index);
    Table->set(Offset + I, ContextPtr, Function, TypeString);
  }
}

namespace runtime {
void WebAssemblyTable::set(
    std::uint32_t Index, __sable_instance_t *ContextPtr,
    __sable_function_t *FunctionPtr, std::string_view Signature) {
  assert(Index < Storage.size());
  TableEntry Entry{
      .ContextPtr = ContextPtr,
      .FunctionPtr = FunctionPtr,
      .Signature = std::string(Signature)};
  Storage[Index] = Entry;
}

__sable_instance_t *WebAssemblyTable::getContextPtr(std::uint32_t Index) const {
  assert(Index < Storage.size());
  return Storage[Index].ContextPtr;
}

__sable_function_t *
WebAssemblyTable::getFunctionPtr(std::uint32_t Index) const {
  assert(Index < Storage.size());
  return Storage[Index].FunctionPtr;
}

std::string_view WebAssemblyTable::getSignature(std::uint32_t Index) const {
  assert(Index < Storage.size());
  return Storage[Index].Signature;
}

WebAssemblyTable::WebAssemblyTable(std::uint32_t NumEntries)
    : WebAssemblyTable(NumEntries, NO_MAXIMUM) {}

WebAssemblyTable::WebAssemblyTable(
    std::uint32_t NumEntries, std::uint32_t MaxNumEntries)
    : Size(NumEntries), MaxSize(MaxNumEntries), Storage() {
  TableEntry DefaultEntry{
      .ContextPtr = nullptr,
      .FunctionPtr = nullptr,
      .Signature = std::string()};
  Storage.resize(NumEntries, DefaultEntry);
}

std::uint32_t WebAssemblyTable::getSize() const { return Size; }

bool WebAssemblyTable::hasMaxSize() const { return MaxSize != NO_MAXIMUM; }

std::uint32_t WebAssemblyTable::getMaxSize() const { return MaxSize; }

bool WebAssemblyTable::isNull(std::uint32_t Index) const {
  if (!(Index < getSize()))
    throw exceptions::TableAccessOutOfBound(*this, Index);
  return Storage[Index].FunctionPtr == nullptr;
}

bytecode::FunctionType WebAssemblyTable::getType(std::uint32_t Index) const {
  if (isNull(Index)) throw exceptions::BadTableEntry(*this, Index);
  auto const &Signature = Storage[Index].Signature;
  return detail::fromSignature<bytecode::FunctionType>(Signature);
}

WebAssemblyCallee WebAssemblyTable::get(std::uint32_t Index) const {
  if (isNull(Index)) throw exceptions::BadTableEntry(*this, Index);
  auto *ContextPtr = Storage[Index].ContextPtr;
  auto *FunctionPtr = Storage[Index].FunctionPtr;
  auto *Signature = Storage[Index].Signature.c_str();
  return WebAssemblyCallee(ContextPtr, FunctionPtr, Signature);
}

void WebAssemblyTable::set(std::uint32_t Index, WebAssemblyCallee Callee) {
  auto *ContextPtr = Callee.getContextPtr();
  auto *FunctionPtr = Callee.getFunctionPtr();
  auto *Signature = Callee.getSignature();
  set(Index, ContextPtr, FunctionPtr, Signature);
}

__sable_table_t *WebAssemblyTable::asInstancePtr() {
  return reinterpret_cast<__sable_table_t *>(this);
}

WebAssemblyTable *
WebAssemblyTable::fromInstancePtr(__sable_table_t *InstancePtr) {
  return reinterpret_cast<WebAssemblyTable *>(InstancePtr);
}

} // namespace runtime
