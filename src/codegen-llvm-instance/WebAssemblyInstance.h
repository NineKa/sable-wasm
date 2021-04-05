#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_WEBASSEMBLY_INSTANCE
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_WEBASSEMBLY_INSTANCE

#include "../bytecode/Type.h"
#include "../utility/Commons.h"

#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>

extern "C" {
struct __sable_memory_t;
struct __sable_table_t;
struct __sable_global_t;
struct __sable_function_t;
struct __sable_instance_t;

void __sable_unreachable();
std::uint32_t __sable_memory_size(__sable_memory_t *);
void __sable_memory_guard(__sable_memory_t *, std::uint32_t Offset);
std::uint32_t __sable_memory_grow(__sable_memory_t *, std::uint32_t Delta);

void __sable_table_guard(__sable_table_t *, std::uint32_t);
void __sable_table_check(__sable_table_t *, std::uint32_t, char const *);
__sable_instance_t *
__sable_table_instance_closure(__sable_table_t *, std::uint32_t Index);
__sable_function_t *
__sable_table_function_ptr(__sable_table_t *, std::uint32_t Index);
void __sable_table_set(
    __sable_table_t *, __sable_instance_t *, std::uint32_t StartPos,
    std::uint32_t Count, std::uint32_t Indices[]);
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
bytecode::ValueType fromTypeChar(char TypeChar);
char toTypeChar(bytecode::ValueType const &Type);
bytecode::FunctionType fromTypeString(std::string_view TypeString);
std::string toTypeString(bytecode::FunctionType const &Type);

template <typename T> constexpr char type_char();
template <> constexpr char type_char<std::int32_t>() { return 'I'; }
template <> constexpr char type_char<std::uint32_t>() { return 'I'; }
template <> constexpr char type_char<std::int64_t>() { return 'J'; }
template <> constexpr char type_char<std::uint64_t>() { return 'J'; }
template <> constexpr char type_char<float>() { return 'F'; }
template <> constexpr char type_char<double>() { return 'D'; }

template <typename RetType, typename... ArgTypes>
inline std::string type_str() {
  std::string TypeStr;
  std::array<char, sizeof...(ArgTypes)> ParamTypes{type_char<ArgTypes>()...};
  TypeStr.append(ParamTypes.begin(), ParamTypes.end());
  TypeStr.push_back(':');
  if constexpr (!std::is_same_v<RetType, void>) {
    char ReturnType = type_char<RetType>();
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
  using Entry = std::tuple<
      __sable_instance_t *, // Instance Closure Pointer
      __sable_function_t *, // Function Pointer
      std::string           // Type String
      >;
  std::vector<Entry> Storage;

  friend void ::__sable_table_guard(__sable_table_t *, std::uint32_t);
  friend void ::__sable_table_check(
      __sable_table_t *, std::uint32_t, char const *);
  friend __sable_instance_t * ::__sable_table_instance_closure(
      __sable_table_t *, std::uint32_t);
  friend __sable_function_t * ::__sable_table_function_ptr(
      __sable_table_t *, std::uint32_t Index);
  friend void ::__sable_table_set(
      __sable_table_t *, __sable_instance_t *, std::uint32_t, std::uint32_t,
      std::uint32_t *);

  void
  set(std::uint32_t, __sable_instance_t *, __sable_function_t *,
      std::string_view TypeString);
  __sable_instance_t *getInstanceClosure(std::uint32_t) const;
  __sable_function_t *getFunctionPointer(std::uint32_t) const;
  std::string_view getTypeString(std::uint32_t) const;

  static constexpr std::uint32_t NO_MAXIMUM =
      std::numeric_limits<std::uint32_t>::max();

public:
  explicit WebAssemblyTable(std::uint32_t NumEntries);
  WebAssemblyTable(std::uint32_t NumEntries, std::uint32_t MaxNumEntries);

  std::uint32_t getSize() const;
  bool hasMaxSize() const;
  std::uint32_t getMaxSize() const;

  bool isNull(std::uint32_t Index) const;
  bytecode::FunctionType getType(std::uint32_t Index) const;

  WebAssemblyCallee get(std::uint32_t) const;

  template <typename RetType, typename... ArgTypes>
  void
  set(std::uint32_t Index,
      RetType (*FunctionPointer)(__sable_instance_t *, ArgTypes...)) {
    auto TypeString = detail::type_str<RetType, ArgTypes...>();
    set(Index, nullptr, FunctionPointer, TypeString);
  }

  void set(std::uint32_t, WebAssemblyCallee Callee);

  __sable_table_t *asInstancePtr();
  static WebAssemblyTable *fromInstancePtr(__sable_table_t *InstancePtr);
};

class WebAssemblyInstanceBuilder {
  std::unique_ptr<WebAssemblyInstance> Instance;

  WebAssemblyInstanceBuilder &import(
      std::string_view ModuleName, std::string_view EntityName,
      std::string_view TypeString, std::intptr_t Function);

  bool tryImport(
      std::string_view ModuleName, std::string_view EntityName,
      std::string_view TypeString, std::intptr_t Function);

public:
  explicit WebAssemblyInstanceBuilder(char const *Path);
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
      RetType (*Function)(__sable_instance_t *, ArgTypes...)) {
    auto TypeString = detail::type_str<RetType, ArgTypes...>();
    auto TypeErasedPtr = reinterpret_cast<std::intptr_t>(Function);
    return import(ModuleName, EntityName, TypeString, TypeErasedPtr);
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
      RetType (*Function)(__sable_instance_t *, ArgTypes...)) {
    auto TypeString = detail::type_str<RetType, ArgTypes...>();
    auto TypeErasedPtr = reinterpret_cast<std::intptr_t>(Function);
    return tryImport(ModuleName, EntityName, TypeString, TypeErasedPtr);
  }

  std::unique_ptr<WebAssemblyInstance> Build();
};

class WebAssemblyInstance {
  friend class WebAssemblyInstanceBuilder;
  friend class WebAssemblyMemory;
  void **Storage = nullptr; // __sable_instance_t
  void *DLHandler = nullptr;

  // Exports:
  using FunctionEntry = std::tuple<
      __sable_instance_t *, // Function Instance Closure
      __sable_function_t *, // Function Pointer
      char const *          // Type String
      >;
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
  __sable_instance_t *&getFunctionInstanceClosure(std::size_t Index);
  __sable_function_t *&getFunctionPointer(std::size_t Index);

  WebAssemblyInstance() = default;

  void replace(__sable_memory_t *Old, __sable_memory_t *New);

  friend void ::__sable_table_set(
      __sable_table_t *, __sable_instance_t *, std::uint32_t, std::uint32_t,
      std::uint32_t *);
  WebAssemblyCallee getFunction(std::uint32_t Index);

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
  __sable_instance_t *Instance;
  __sable_function_t *Function;
  char const *ExpectTypeStr;

  friend class WebAssemblyInstance;
  friend class WebAssemblyTable;
  WebAssemblyCallee(
      __sable_instance_t *Instance_, __sable_function_t *Function_,
      char const *ExpectTypeStr_)
      : Instance(Instance_), Function(Function_),
        ExpectTypeStr(ExpectTypeStr_) {}

public:
  __sable_function_t *getFunctionPointer() const { return Function; }
  __sable_instance_t *getInstanceClosurePointer() const { return Instance; }
  char const *getTypeString() const { return ExpectTypeStr; }

  template <typename RetType, typename... ArgTypes>
  RetType invoke(ArgTypes... Args) const { // use copy instead of forward
    using FunctionTy = RetType (*)(__sable_instance_t *, ArgTypes...);
    if (detail::type_str<RetType, ArgTypes...>() != ExpectTypeStr) {
      throw std::runtime_error("type mismatch");
    }
    auto *CastedPtr = reinterpret_cast<FunctionTy>(Function);
    return CastedPtr(Instance, Args...);
  }
};
} // namespace runtime

#endif
