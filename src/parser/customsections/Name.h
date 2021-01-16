#ifndef SABLE_INCLUDE_GUARD_PARSER_CUSTOM_SECTIONS_NAME
#define SABLE_INCLUDE_GUARD_PARSER_CUSTOM_SECTIONS_NAME

#include "../Parser.h"

#include <map>
#include <string>

namespace parser::customsections {
class Name : public parser::CustomSection {
  struct NameEntry {
    std::string FuncName;
    std::map<bytecode::LocalIDX, std::string> LocalNames;
  };
  std::map<bytecode::FuncIDX, NameEntry> Entries;
  std::optional<std::string> ModuleName;

public:
  Name() : parser::CustomSection("name") {}
  void parse(std::span<std::byte const> Payload) override;

  std::string_view getModuleName() const;
  std::string_view getFunctionName(bytecode::FuncIDX Func) const;
  std::string_view
  getLocalName(bytecode::FuncIDX Func, bytecode::LocalIDX Local) const;

  auto getFunctionNames() const;
};

inline auto Name::getFunctionNames() const {
  return Entries | ranges::views::transform([](auto const &EntryPair) {
           auto FuncIDX = std::get<0>(EntryPair);
           std::string_view FuncName(std::get<1>(EntryPair).FuncName);
           return std::make_pair(FuncIDX, FuncName);
         });
}

} // namespace parser::customsections

#endif