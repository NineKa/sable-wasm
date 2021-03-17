#include "../bytecode/Type.h"
#include "../utility/Commons.h"

#include <fmt/format.h>
#include <range/v3/view/subrange.hpp>

#include <cstdint>
#include <limits>
#include <vector>

struct ImportDescriptor {
  std::uint32_t Index;
  char const *ModuleName;
  char const *EntityName;
};

struct ExportDescriptor {
  std::uint32_t Index;
  char const *Name;
};

struct MemoryMetadata {
  struct MemoryType {
    std::uint32_t Min;
    std::uint32_t Max;
  };
  std::uint32_t Size, ISize, ESize;
  MemoryType const *Entities;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
  MemoryType const &operator[](std::uint32_t I) const { return Entities[I]; }
  auto imports() const { return ranges::subrange(Imports, Imports + ISize); }
  auto exports() const { return ranges::subrange(Exports, Exports + ESize); }
};

struct FunctionMetadata {
  using FunctionType = char const *;
  std::uint32_t Size, ISize, ESize;
  FunctionType const *Entities;
  ImportDescriptor const *Imports;
  ExportDescriptor const *Exports;
  FunctionType const &operator[](std::uint32_t I) const { return Entities[I]; }
  auto imports() const { return ranges::subrange(Imports, Imports + ISize); }
  auto exports() const { return ranges::subrange(Exports, Exports + ESize); }
};

bytecode::ValueType convert(char TypeChar) {
  switch (toupper(TypeChar)) {
  case 'I': return bytecode::valuetypes::I32;
  case 'J': return bytecode::valuetypes::I64;
  case 'F': return bytecode::valuetypes::F32;
  case 'D': return bytecode::valuetypes::F64;
  default: SABLE_UNREACHABLE();
  }
}

bytecode::FunctionType convert(char const *TypeString) {
  std::vector<bytecode::ValueType> ParamTypes;
  std::vector<bytecode::ValueType> ResultTypes;
  bool SeenSeparator = false;
  for (char const *I = TypeString; *I != '\0'; ++I) {
    if (*I == ':') {
      if (SeenSeparator) SABLE_UNREACHABLE();
      SeenSeparator = true;
      continue;
    }
    auto ValueType = convert(*I);
    if (!SeenSeparator) {
      ParamTypes.push_back(ValueType);
    } else {
      ResultTypes.push_back(ValueType);
    }
  }
  return bytecode::FunctionType(std::move(ParamTypes), std::move(ResultTypes));
}

extern "C" {
extern MemoryMetadata const __sable_memory_metadata;
extern FunctionMetadata const __sable_function_metadata;

void __sable_initialize(void *);
}

namespace fmt {
template <> struct formatter<MemoryMetadata::MemoryType> {
  template <typename C> auto parse(C &&CTX) { return CTX.begin(); }
  template <typename C>
  auto format(MemoryMetadata::MemoryType const &Type, C &&CTX) {
    if (Type.Max == std::numeric_limits<std::uint32_t>::max())
      return fmt::format_to(CTX.out(), "{{min {}}}", Type.Min);
    return fmt::format_to(CTX.out(), "{{min {}, max{}}}", Type.Min, Type.Max);
  }
};
} // namespace fmt

int main(int argc, char const *argv[]) {
  utility::ignore(argc);
  utility::ignore(argv);

  for (auto const &Export : __sable_memory_metadata.exports()) {
    auto const &Type = __sable_memory_metadata[Export.Index];
    fmt::print("{} :: {}\n", Export.Name, Type);
  }
  for (auto const &Import : __sable_function_metadata.imports()) {
    auto Type = convert(__sable_function_metadata[Import.Index]);
    fmt::print("{}::{} :: {}\n", Import.ModuleName, Import.EntityName, Type);
  }

  int32_t a, b, c;
  void *Instance[20];
  Instance[6] = &a;
  Instance[7] = &b;
  Instance[8] = &c;
  __sable_initialize(Instance);

  fmt::print("{}\n{}\n{}\n", a, b, c);

  return 0;
}