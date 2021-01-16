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
  std::vector<std::pair<bytecode::FuncIDX, std::string>> FunctionNames;
  std::vector<std::tuple<bytecode::FuncIDX, bytecode::LocalIDX, std::string>>
      LocalNames;

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
