#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_WEBASSEMBLY_INSTANCE
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_WEBASSEMBLY_INSTANCE

#include "../bytecode/Type.h"
#include "../utility/Commons.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>

extern "C" {
struct __sable_memory_t;
struct __sable_table_t;
struct __sable_global_t;
struct __sable_function_t;
struct __sable_instance_t;

// clang-format off
void __sable_unreachable();

std::uint32_t __sable_memory_size(__sable_memory_t *);
void __sable_memory_guard(__sable_memory_t *, std::uint32_t Offset);
std::uint32_t __sable_memory_grow(__sable_memory_t *, std::uint32_t Delta);

void __sable_table_guard(__sable_table_t *, std::uint32_t Index);
void __sable_table_check(__sable_table_t *, std::uint32_t Index, char const *ExpectSignature);
__sable_instance_t *__sable_table_context(__sable_table_t *, std::uint32_t Index);
__sable_function_t *__sable_table_function(__sable_table_t *, std::uint32_t Index);
void __sable_table_set(__sable_table_t *, __sable_instance_t *, std::uint32_t Offset, std::uint32_t Count, std::uint32_t Indices[]);
// clang-format on
}

namespace runtime {
class WebAssemblyMemory;
class WebAssemblyGlobal;
class WebAssemblyTable;
class WebAssemblyCallee;
class WebAssemblyInstance;
class WebAssemblyInstanceBuilder;

namespace exceptions {
class MalformedInstanceLibrary : public std::runtime_error {
public:
  MalformedInstanceLibrary(char const *What) : std::runtime_error(What) {}
};

class Unreachable : public std::runtime_error {
public:
  Unreachable() : std::runtime_error("unreachable") {}
};

class MemoryAccessOutOfBound : public std::runtime_error {
  WebAssemblyMemory const *Site;
  std::size_t AttemptOffset;

public:
  MemoryAccessOutOfBound(
      WebAssemblyMemory const &Site_, std::size_t AttemptOffset_)
      : std::runtime_error("WebAssembly memory instance access out of bound"),
        Site(std::addressof(Site_)), AttemptOffset(AttemptOffset_) {}
  WebAssemblyMemory const &getSite() const { return *Site; }
  std::size_t getAttemptOffset() const { return AttemptOffset; }
};

class TableAccessOutOfBound : public std::runtime_error {
  WebAssemblyTable const *Site;
  std::uint32_t AttemptIndex;

public:
  TableAccessOutOfBound(
      WebAssemblyTable const &Site_, std::uint32_t AttemptIndex_)
      : std::runtime_error("WebAssembly table instance access out of bound"),
        Site(std::addressof(Site_)), AttemptIndex(AttemptIndex_) {}
  WebAssemblyTable const &getSite() const { return *Site; }
  std::uint32_t getAttemptIndex() const { return AttemptIndex; }
};

class TableTypeMismatch : public std::runtime_error {
  WebAssemblyTable const *Site;
  std::uint32_t AttemptIndex;
  bytecode::FunctionType ExpectType;
  bytecode::FunctionType ActualType;

public:
  TableTypeMismatch(
      WebAssemblyTable const &Site_, std::uint32_t AttemptIndex_,
      bytecode::FunctionType ExpectType_, bytecode::FunctionType ActualType_)
      : std::runtime_error("WebAssembly table type mismatch"),
        Site(std::addressof(Site_)), AttemptIndex(AttemptIndex_),
        ExpectType(std::move(ExpectType_)), ActualType(std::move(ActualType_)) {
  }
  WebAssemblyTable const &getSite() const { return *Site; }
  std::uint32_t getAttemptIndex() const { return AttemptIndex; }
  bytecode::FunctionType const &getExpectType() const { return ExpectType; }
  bytecode::FunctionType const &getActualType() const { return ActualType; }
};

class GlobalTypeMismatch : public std::runtime_error {
  WebAssemblyGlobal const *Site;
  bytecode::ValueType AttemptType;

public:
  GlobalTypeMismatch(
      WebAssemblyGlobal const &Site_, bytecode::ValueType AttemptType_)
      : std::runtime_error("WebAssembly global type mismatch"),
        Site(std::addressof(Site_)), AttemptType(AttemptType_) {}
  WebAssemblyGlobal const &getSite() const { return *Site; }
  bytecode::ValueType const &getAttemptType() const { return AttemptType; }
};

class BadTableEntry : public std::runtime_error {
  WebAssemblyTable const *Site;
  std::uint32_t AttemptIndex;

public:
  BadTableEntry(WebAssemblyTable const &Site_, std::uint32_t AttemptIndex_)
      : std::runtime_error("bad WebAssembly table entry"),
        Site(std::addressof(Site_)), AttemptIndex(AttemptIndex_) {}
  WebAssemblyTable const &getSite() const { return *Site; }
  std::uint32_t getAttemptIndex() const { return AttemptIndex; }
};
} // namespace exceptions

namespace detail {
template <typename T> struct from_signature_parameter;
template <typename T> struct to_signature_result;
// clang-format off
template <> struct from_signature_parameter<bytecode::ValueType>
{ using type = char; };
template <> struct from_signature_parameter<bytecode::GlobalType>
{ using type = char; };
template <> struct from_signature_parameter<bytecode::FunctionType>
{ using type = std::string_view; };
// clang-format on
template <typename T>
T fromSignature(typename from_signature_parameter<T>::type);
template <>
bytecode::ValueType fromSignature<bytecode::ValueType>(char Signature);
template <>
bytecode::GlobalType fromSignature<bytecode::GlobalType>(char Signature);
template <>
bytecode::FunctionType
fromSignature<bytecode::FunctionType>(std::string_view Signature);

char toSignature(bytecode::ValueType const &Type);
char toSignature(bytecode::GlobalType const &Type);
std::string toSignature(bytecode::FunctionType const &Type);

template <typename T> constexpr char signature_();
template <> constexpr char signature_<std::int32_t>() { return 'I'; }
template <> constexpr char signature_<std::uint32_t>() { return 'I'; }
template <> constexpr char signature_<std::int64_t>() { return 'J'; }
template <> constexpr char signature_<std::uint64_t>() { return 'J'; }
template <> constexpr char signature_<float>() { return 'F'; }
template <> constexpr char signature_<double>() { return 'D'; }

template <typename RetType, typename... ArgTypes>
inline std::string signature() {
  std::string TypeStr;
  std::array<char, sizeof...(ArgTypes)> ParamTypes{signature_<ArgTypes>()...};
  TypeStr.append(ParamTypes.begin(), ParamTypes.end());
  TypeStr.push_back(':');
  if constexpr (!std::is_same_v<RetType, void>) {
    char ReturnType = signature_<RetType>();
    TypeStr.push_back(ReturnType);
  }
  return TypeStr;
}
} // namespace detail

class WebAssemblyMemory {
  friend class WebAssemblyInstance;
  friend class WebAssemblyInstanceBuilder;
  struct MemoryMetadata;
  std::byte *Memory;

  MemoryMetadata &getMetadata();
  MemoryMetadata const &getMetadata() const;

  void addUseSite(WebAssemblyInstance &Instance);
  void removeUseSite(WebAssemblyInstance &Instance);

  static constexpr std::uint32_t NO_MAXIMUM =
      std::numeric_limits<std::uint32_t>::max();

public:
  explicit WebAssemblyMemory(std::uint32_t NumPage);
  WebAssemblyMemory(std::uint32_t NumPage, std::uint32_t MaxNumPage);
  WebAssemblyMemory(WebAssemblyMemory const &) = delete;
  WebAssemblyMemory(WebAssemblyMemory &&) noexcept = delete;
  WebAssemblyMemory &operator=(WebAssemblyMemory const &) = delete;
  WebAssemblyMemory &operator=(WebAssemblyMemory &&) noexcept = delete;
  ~WebAssemblyMemory() noexcept;

  bool hasMaxSize() const;
  std::uint32_t getMaxSize() const;
  std::uint32_t getSize() const;
  std::size_t getSizeInBytes() const;

  std::byte *data();
  std::byte const *data() const;

  std::uint32_t grow(std::uint32_t DeltaNumPage);

  std::byte &operator[](std::size_t Offset);
  std::byte const &operator[](std::size_t Offset) const;
  std::byte &get(std::size_t Offset);
  std::byte const &get(std::size_t Offset) const;
  std::span<std::byte> get(std::size_t Offset, std::size_t Length);
  std::span<std::byte const> get(std::size_t Offset, std::size_t Length) const;

  static std::size_t getWebAssemblyPageSize();
  static std::size_t getNativePageSize();

  __sable_memory_t *asInstancePtr();
  static WebAssemblyMemory *fromInstancePtr(__sable_memory_t *InstancePtr);
  static std::uint32_t const GrowFailed;
};

class WebAssemblyGlobal {
  union {
    std::int32_t I32;
    std::int64_t I64;
    float F32;
    double F64;
  } Storage;
  bytecode::ValueType ValueType;

public:
  explicit WebAssemblyGlobal(bytecode::ValueType Type_);
  WebAssemblyGlobal(WebAssemblyGlobal const &) = delete;
  WebAssemblyGlobal(WebAssemblyGlobal &&) noexcept = delete;
  WebAssemblyGlobal &operator=(WebAssemblyGlobal const &) = delete;
  WebAssemblyGlobal &operator=(WebAssemblyGlobal &&) noexcept = delete;
  ~WebAssemblyGlobal() noexcept = default;

  bytecode::ValueType const &getValueType() const;
  std::int32_t &asI32();
  std::int64_t &asI64();
  float &asF32();
  double &asF64();
  std::int32_t const &asI32() const;
  std::int64_t const &asI64() const;
  float const &asF32() const;
  double const &asF64() const;

  __sable_global_t *asInstancePtr();
  static WebAssemblyGlobal *fromInstancePtr(__sable_global_t *InstancePtr);
};

class WebAssemblyTable {
  std::uint32_t Size;
  std::uint32_t MaxSize;

  struct TableEntry {
    __sable_instance_t *ContextPtr;
    __sable_function_t *FunctionPtr;
    std::string Signature;
  };
  std::vector<TableEntry> Storage;

  // clang-format off
  friend void ::__sable_table_guard(__sable_table_t *, std::uint32_t);
  friend void ::__sable_table_check(__sable_table_t *, std::uint32_t, char const *);
  friend __sable_instance_t * ::__sable_table_context(__sable_table_t *, std::uint32_t);
  friend __sable_function_t * ::__sable_table_function(__sable_table_t *, std::uint32_t);
  friend void ::__sable_table_set(__sable_table_t *, __sable_instance_t *, std::uint32_t, std::uint32_t, std::uint32_t *);
  // clang-format on

  void
  set(std::uint32_t, __sable_instance_t *ContextPtr,
      __sable_function_t *FunctionPtr, std::string_view Signature);
  __sable_instance_t *getContextPtr(std::uint32_t) const;
  __sable_function_t *getFunctionPtr(std::uint32_t) const;
  std::string_view getSignature(std::uint32_t) const;

  static constexpr std::uint32_t NO_MAXIMUM =
      std::numeric_limits<std::uint32_t>::max();

public:
  explicit WebAssemblyTable(std::uint32_t NumEntries);
  WebAssemblyTable(std::uint32_t NumEntries, std::uint32_t MaxNumEntries);
  WebAssemblyTable(WebAssemblyTable const &) = delete;
  WebAssemblyTable(WebAssemblyTable &&) noexcept = delete;
  WebAssemblyTable &operator=(WebAssemblyTable const &) = delete;
  WebAssemblyTable &operator=(WebAssemblyTable &&) noexcept = delete;
  ~WebAssemblyTable() noexcept = default;

  std::uint32_t getSize() const;
  bool hasMaxSize() const;
  std::uint32_t getMaxSize() const;

  bool isNull(std::uint32_t Index) const;
  bytecode::FunctionType getType(std::uint32_t Index) const;

  WebAssemblyCallee get(std::uint32_t) const;

  template <typename RetType, typename... ArgTypes>
  void
  set(std::uint32_t Index,
      RetType (*FunctionPtr)(__sable_instance_t *, ArgTypes...)) {
    auto Signature = detail::signature<RetType, ArgTypes...>();
    auto *TypeErasedPtr = reinterpret_cast<__sable_function_t *>(FunctionPtr);
    set(Index, nullptr, TypeErasedPtr, Signature);
  }

  void set(std::uint32_t, WebAssemblyCallee Callee);

  __sable_table_t *asInstancePtr();
  static WebAssemblyTable *fromInstancePtr(__sable_table_t *InstancePtr);
};

class WebAssemblyInstanceBuilder {
  std::unique_ptr<WebAssemblyInstance> Instance;

  WebAssemblyInstanceBuilder &import(
      std::string_view ModuleName, std::string_view EntityName,
      std::string_view Signature, std::intptr_t Function);

  bool tryImport(
      std::string_view ModuleName, std::string_view EntityName,
      std::string_view Signature, std::intptr_t Function);

public:
  explicit WebAssemblyInstanceBuilder(std::filesystem::path const &Path);
  WebAssemblyInstanceBuilder(WebAssemblyInstanceBuilder const &) = delete;
  WebAssemblyInstanceBuilder(WebAssemblyInstanceBuilder &&) noexcept = delete;
  WebAssemblyInstanceBuilder &
  operator=(WebAssemblyInstanceBuilder const &) = delete;
  WebAssemblyInstanceBuilder &
  operator=(WebAssemblyInstanceBuilder &&) noexcept = delete;
  ~WebAssemblyInstanceBuilder() noexcept = default;

  WebAssemblyInstanceBuilder &import(
      std::string_view ModuleName, std::string_view EntityName,
      WebAssemblyMemory &Memory);
  WebAssemblyInstanceBuilder &import(
      std::string_view ModuleName, std::string_view EntityName,
      WebAssemblyTable &Table);
  WebAssemblyInstanceBuilder &import(
      std::string_view ModuleName, std::string_view EntityName,
      WebAssemblyGlobal &Global);
  WebAssemblyInstanceBuilder &import(
      std::string_view ModuleName, std::string_view EntityName,
      WebAssemblyCallee Callee);

  template <typename RetType, typename... ArgTypes>
  WebAssemblyInstanceBuilder &import(
      std::string_view ModuleName, std::string_view EntityName,
      RetType (*FunctionPtr)(__sable_instance_t *, ArgTypes...)) {
    auto Signature = detail::signature<RetType, ArgTypes...>();
    auto TypeErasedPtr = reinterpret_cast<std::intptr_t>(FunctionPtr);
    return import(ModuleName, EntityName, Signature, TypeErasedPtr);
  }

  bool tryImport(
      std::string_view ModuleName, std::string_view EntityName,
      WebAssemblyMemory &Memory);
  bool tryImport(
      std::string_view ModuleName, std::string_view EntityName,
      WebAssemblyTable &Table);
  bool tryImport(
      std::string_view ModuleName, std::string_view EntityName,
      WebAssemblyGlobal &Global);
  bool tryImport(
      std::string_view ModuleName, std::string_view EntityName,
      WebAssemblyCallee Callee);

  template <typename RetType, typename... ArgTypes>
  bool tryImport(
      std::string_view ModuleName, std::string_view EntityName,
      RetType (*FunctionPtr)(__sable_instance_t *, ArgTypes...)) {
    auto Signature = detail::signature<RetType, ArgTypes...>();
    auto TypeErasedPtr = reinterpret_cast<std::intptr_t>(FunctionPtr);
    return tryImport(ModuleName, EntityName, Signature, TypeErasedPtr);
  }

  std::unique_ptr<WebAssemblyInstance> Build();
};

class WebAssemblyInstance {
  friend class WebAssemblyInstanceBuilder;
  friend class WebAssemblyMemory;
  void **Storage = nullptr; // __sable_instance_t
  void *DLHandler = nullptr;

  // Exports:
  struct FunctionEntry {
    __sable_instance_t *ContextPtr;
    __sable_function_t *FunctionPtr;
    char const *Signature;
  };
  std::unordered_map<std::string_view, __sable_memory_t *> ExportedMemories;
  std::unordered_map<std::string_view, __sable_table_t *> ExportedTables;
  std::unordered_map<std::string_view, __sable_global_t *> ExportedGlobals;
  std::unordered_map<std::string_view, FunctionEntry> ExportedFunctions;

  struct ImportDescriptor;
  struct ExportDescriptor;
  struct MemoryMetadata;   // __sable_memory_metadata_t
  struct TableMetadata;    // __sable_table_metadata_t
  struct GlobalMetadata;   // __sable_global_metadata_t
  struct FunctionMetadata; // __sable_function_metadata_t

  MemoryMetadata const &getMemoryMetadata() const;
  TableMetadata const &getTableMetadata() const;
  GlobalMetadata const &getGlobalMetadata() const;
  FunctionMetadata const &getFunctionMetadata() const;

  __sable_memory_t *&getMemory(std::size_t Index);
  __sable_table_t *&getTable(std::size_t Index);
  __sable_global_t *&getGlobal(std::size_t Index);
  __sable_instance_t *&getContextPtr(std::size_t Index);
  __sable_function_t *&getFunctionPtr(std::size_t Index);
  char const *getSignature(std::size_t Index) const;

  WebAssemblyInstance() = default;

  void replace(__sable_memory_t *Old, __sable_memory_t *New);

  // clang-format off
  friend void ::__sable_table_set(__sable_table_t *, __sable_instance_t *, std::uint32_t, std::uint32_t, std::uint32_t *);
  // clang-format on

public:
  WebAssemblyInstance(WebAssemblyInstance const &) = delete;
  WebAssemblyInstance(WebAssemblyInstance &&) noexcept = delete;
  WebAssemblyInstance &operator=(WebAssemblyInstance const &) = delete;
  WebAssemblyInstance &operator=(WebAssemblyInstance &&) noexcept = delete;
  ~WebAssemblyInstance() noexcept;

  WebAssemblyMemory &getMemory(std::string_view Name);
  WebAssemblyTable &getTable(std::string_view Name);
  WebAssemblyGlobal &getGlobal(std::string_view Name);
  WebAssemblyCallee getFunction(std::string_view Name);

  WebAssemblyMemory *tryGetMemory(std::string_view Name);
  WebAssemblyTable *tryGetTable(std::string_view Name);
  WebAssemblyGlobal *tryGetGlobal(std::string_view Name);
  std::optional<WebAssemblyCallee> tryGetFunction(std::string_view Name);

  __sable_instance_t *asInstancePtr();
  static WebAssemblyInstance *fromInstancePtr(__sable_instance_t *InstancePtr);
};

class WebAssemblyCallee {
  __sable_instance_t *ContextPtr;
  __sable_function_t *FunctionPtr;
  char const *Signature;

  friend class WebAssemblyInstance;
  friend class WebAssemblyTable;
  WebAssemblyCallee(
      __sable_instance_t *ContextPtr_, __sable_function_t *FunctionPtr_,
      char const *Signature_)
      : ContextPtr(ContextPtr_), FunctionPtr(FunctionPtr_),
        Signature(Signature_) {}

public:
  __sable_function_t *getFunctionPtr() const { return FunctionPtr; }
  __sable_instance_t *getContextPtr() const { return ContextPtr; }
  char const *getSignature() const { return Signature; }

  template <typename RetType, typename... ArgTypes>
  RetType invoke(ArgTypes... Args) const { // use copy instead of forward
    using FunctionTy = RetType (*)(__sable_instance_t *, ArgTypes...);
    if (detail::signature<RetType, ArgTypes...>() != Signature) {
      throw std::runtime_error("type mismatch");
    }
    auto *CastedPtr = reinterpret_cast<FunctionTy>(FunctionPtr);
    return CastedPtr(ContextPtr, Args...);
  }
};
} // namespace runtime

#endif
