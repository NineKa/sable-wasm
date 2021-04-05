#include "WebAssemblyInstance.h"

#include <sys/mman.h>
#include <unistd.h>

#include <range/v3/algorithm/find.hpp>

#include <cassert>
#include <forward_list>
#include <limits>

extern "C" {
std::uint32_t __sable_memory_size(__sable_memory_t *Memory) {
  auto *MemoryInstance = runtime::WebAssemblyMemory::fromInstancePtr(Memory);
  return MemoryInstance->getSize();
}

void __sable_memory_guard(__sable_memory_t *Memory, std::uint32_t Offset) {
  auto *MemoryInstance = runtime::WebAssemblyMemory::fromInstancePtr(Memory);
  if (!(Offset <= MemoryInstance->getSizeInBytes()))
    throw runtime::exceptions::MemoryAccessOutOfBound(*MemoryInstance, Offset);
}

std::uint32_t
__sable_memory_grow(__sable_memory_t *Memory, std::uint32_t Delta) {
  auto *MemoryInstance = runtime::WebAssemblyMemory::fromInstancePtr(Memory);
  return MemoryInstance->grow(Delta);
}
}

namespace runtime {
struct WebAssemblyMemory::MemoryMetadata {
  std::uint32_t Size;      // In Unit of WebAssembly Pages
  std::uint32_t Max;       // In Unit of WebAssembly Pages
  std::size_t SizeInBytes; // In Unit of Bytes
  std::forward_list<WebAssemblyInstance *> *UseSites;
  WebAssemblyMemory *Instance;
};

WebAssemblyMemory::MemoryMetadata &WebAssemblyMemory::getMetadata() {
  auto *Ptr = std::addressof(Memory[-getNativePageSize()]);
  return *reinterpret_cast<MemoryMetadata *>(Ptr);
}

WebAssemblyMemory::MemoryMetadata const &
WebAssemblyMemory::getMetadata() const {
  auto *Ptr = std::addressof(Memory[-getNativePageSize()]);
  return *reinterpret_cast<MemoryMetadata const *>(Ptr);
}

void WebAssemblyMemory::addUseSite(WebAssemblyInstance &Instance) {
  getMetadata().UseSites->push_front(std::addressof(Instance));
}

namespace {
template <typename Iterator, typename T>
Iterator find_before(Iterator BeforeFist, Iterator Last, T const &Value) {
  assert(BeforeFist != Last);
  auto Next = std::next(BeforeFist);
  if (Next == Last) return Last;
  if (*Next == Value) return BeforeFist;
  return find_before(Next, Last, Value);
}
} // namespace

void WebAssemblyMemory::removeUseSite(WebAssemblyInstance &Instance) {
  auto SearchIter = find_before(
      getMetadata().UseSites->before_begin(), getMetadata().UseSites->end(),
      std::addressof(Instance));
  assert(SearchIter != getMetadata().UseSites->end());
  getMetadata().UseSites->erase_after(SearchIter);
}

WebAssemblyMemory::WebAssemblyMemory(std::uint32_t NumPage)
    : WebAssemblyMemory(NumPage, NO_MAXIMUM) {}

WebAssemblyMemory::WebAssemblyMemory(
    std::uint32_t NumPage, std::uint32_t MaxNumPage)
    : Memory(nullptr) {
  assert(getWebAssemblyPageSize() >= getNativePageSize());
  assert(getWebAssemblyPageSize() % getNativePageSize() == 0);
  assert(sizeof(MemoryMetadata) < getNativePageSize());
  assert(NumPage <= MaxNumPage);
  auto NumAllocPage =
      (NumPage * getWebAssemblyPageSize() / getNativePageSize()) + 1;
  auto AllocSize = NumAllocPage * getNativePageSize();
  auto Permission = PROT_READ | PROT_WRITE;
  auto Flag = MAP_PRIVATE | MAP_ANONYMOUS;
  auto *MappedPages = mmap(NULL, AllocSize, Permission, Flag, -1, 0);
  if (MappedPages == nullptr) throw std::bad_alloc();
  Memory = &reinterpret_cast<std::byte *>(MappedPages)[getNativePageSize()];
  getMetadata().Size = NumPage;
  getMetadata().Max = MaxNumPage;
  getMetadata().SizeInBytes = getSize() * getWebAssemblyPageSize();
  getMetadata().Instance = this;
  getMetadata().UseSites = new std::forward_list<WebAssemblyInstance *>();
}

WebAssemblyMemory::~WebAssemblyMemory() noexcept {
  assert(getMetadata().UseSites->empty());
  delete getMetadata().UseSites;
  auto *MappedPages = std::addressof(Memory[-getNativePageSize()]);
  auto MappedSize = getMetadata().SizeInBytes + getNativePageSize();
  munmap(MappedPages, MappedSize);
}

bool WebAssemblyMemory::hasMaxSize() const {
  return getMetadata().Max == NO_MAXIMUM;
}

std::uint32_t WebAssemblyMemory::getMaxSize() const {
  return getMetadata().Max;
}

std::uint32_t WebAssemblyMemory::getSize() const { return getMetadata().Size; }

std::size_t WebAssemblyMemory::getSizeInBytes() const {
  return getMetadata().SizeInBytes;
}

std::byte *WebAssemblyMemory::data() { return Memory; }

std::byte const *WebAssemblyMemory::data() const { return Memory; }

std::uint32_t WebAssemblyMemory::grow(std::uint32_t DeltaNumPage) {
  auto *OldInstancePtr = asInstancePtr();
  auto OldSize = getSize();
  if (getMetadata().Size + DeltaNumPage > getMetadata().Max) return GrowFailed;
  auto *MappedPages = &Memory[-getNativePageSize()];
  auto MappedSize = getMetadata().SizeInBytes + getNativePageSize();
  auto NewMappedSize = MappedSize + DeltaNumPage * getWebAssemblyPageSize();
  auto *RemappedPages =
      mremap(MappedPages, MappedSize, NewMappedSize, MREMAP_MAYMOVE);
  if (RemappedPages == (void *)-1) return GrowFailed;
  Memory = &reinterpret_cast<std::byte *>(RemappedPages)[getNativePageSize()];
  getMetadata().Size = getMetadata().Size + DeltaNumPage;
  getMetadata().SizeInBytes = getSize() * getWebAssemblyPageSize();
  auto *NewInstancePtr = asInstancePtr();
  for (auto *UseSite : *getMetadata().UseSites)
    UseSite->replace(OldInstancePtr, NewInstancePtr);
  return OldSize;
}

__sable_memory_t *WebAssemblyMemory::asInstancePtr() {
  return reinterpret_cast<__sable_memory_t *>(data()); // NOLINT
}

std::size_t WebAssemblyMemory::getWebAssemblyPageSize() {
  return 64 * 1024; /* 64 KiB */
}

std::size_t WebAssemblyMemory::getNativePageSize() {
  return sysconf(_SC_PAGESIZE);
}

std::byte &WebAssemblyMemory::operator[](std::size_t Offset) {
  return data()[Offset];
}

std::byte const &WebAssemblyMemory::operator[](std::size_t Offset) const {
  return data()[Offset];
}

std::byte &WebAssemblyMemory::get(std::size_t Offset) {
  if (!(Offset < getSize() * getWebAssemblyPageSize()))
    throw exceptions::MemoryAccessOutOfBound(*this, Offset);
  return this->operator[](Offset);
}

std::byte const &WebAssemblyMemory::get(std::size_t Offset) const {
  if (!(Offset < getSize() * getWebAssemblyPageSize()))
    throw exceptions::MemoryAccessOutOfBound(*this, Offset);
  return this->operator[](Offset);
}

std::span<std::byte>
WebAssemblyMemory::get(std::size_t Offset, std::size_t Length) {
  if (!((Offset < getSize() * getWebAssemblyPageSize()) &&
        ((Offset + Length < getSize() * getWebAssemblyPageSize()))))
    throw exceptions::MemoryAccessOutOfBound(*this, Offset);
  return std::span<std::byte>(&this->operator[](Offset), Length);
}

std::span<std::byte const>
WebAssemblyMemory::get(std::size_t Offset, std::size_t Length) const {
  if (!((Offset < getSize() * getWebAssemblyPageSize()) &&
        ((Offset + Length < getSize() * getWebAssemblyPageSize()))))
    throw exceptions::MemoryAccessOutOfBound(*this, Offset);
  return std::span<std::byte const>(&this->operator[](Offset), Length);
}

WebAssemblyMemory *
WebAssemblyMemory::fromInstancePtr(__sable_memory_t *InstancePtr) {
  if (InstancePtr == nullptr) return nullptr;
  auto *StartAddress =
      &reinterpret_cast<std::byte *>(InstancePtr)[-getNativePageSize()];
  auto *Metadata = reinterpret_cast<MemoryMetadata *>(StartAddress);
  return Metadata->Instance;
}

std::uint32_t const WebAssemblyMemory::GrowFailed =
    static_cast<std::uint32_t>(-1);
} // namespace runtime
