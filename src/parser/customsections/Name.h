#ifndef SABLE_INCLUDE_GUARD_PARSER_CUSTOM_SECTIONS_NAME
#define SABLE_INCLUDE_GUARD_PARSER_CUSTOM_SECTIONS_NAME

#include "../ByteArrayReader.h"
#include "../Parser.h"
#include "../Reader.h"

#include <range/v3/all.hpp>

#include <map>
#include <optional>
#include <string>

namespace parser::customsections {
class Name : public parser::CustomSection {
  std::optional<std::string> ModuleName;

  struct FunctionNameEntry {
    bytecode::FuncIDX FuncIndex;
    std::string Name;
    bool operator==(FunctionNameEntry const &) const = default;
    auto operator<=>(FunctionNameEntry const &) const = default;
  };

  struct LocalNameEntry {
    bytecode::FuncIDX FuncIndex;
    bytecode::LocalIDX LocalIndex;
    std::string Name;
    bool operator==(LocalNameEntry const &) const = default;
    auto operator<=>(LocalNameEntry const &) const = default;
  };

  std::vector<FunctionNameEntry> FunctionNames;
  std::vector<LocalNameEntry> LocalNames;

  void parseModuleName(WASMReader<ByteArrayReader> &Reader);
  void parseFunctionNames(WASMReader<ByteArrayReader> &Reader);
  void parseLocalNames(WASMReader<ByteArrayReader> &Reader);

public:
  Name() : parser::CustomSection("name") {}
  void parse(std::span<std::byte const> Payload) override;

  std::optional<std::string_view> getModuleName() const;
  std::optional<std::string_view> getFunctionName(bytecode::FuncIDX Func) const;
  std::optional<std::string_view>
  getLocalName(bytecode::FuncIDX Func, bytecode::LocalIDX Local) const;

  auto getFunctionNames() const { return ranges::views::all(FunctionNames); }
  auto getLocalNames() const { return ranges::views::all(LocalNames); }
};

} // namespace parser::customsections

#endif
