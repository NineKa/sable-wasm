#include "Name.h"

namespace parser::customsections {
void Name::parse(std::span<std::byte const> Payload) {
  utility::ignore(Payload);
}

std::string_view Name::getModuleName() const { return ""; }

std::string_view Name::getFunctionName(bytecode::FuncIDX Func) const {
  utility::ignore(Func);
  return "";
}

std::string_view
Name::getLocalName(bytecode::FuncIDX Func, bytecode::LocalIDX Local) const {
  utility::ignore(Func, Local);
  return "";
}

} // namespace parser::customsections