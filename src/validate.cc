#include "bytecode/Validation.h"
#include "parser/ByteArrayReader.h"
#include "parser/ModuleBuilderDelegate.h"
#include "parser/Parser.h"
#include "parser/customsections/Name.h"

#include <mio/mmap.hpp>

#include <filesystem>

[[noreturn]] void panic(std::string_view Message) {
  fmt::print("{}\n", Message);
  std::exit(1);
}

void validate(std::filesystem::path const &Path) {
  mio::basic_mmap_source<std::byte> Source(Path.c_str());
  parser::ByteArrayReader Reader(Source);
  parser::ModuleBuilderDelegate ModuleBuilderDelegate;
  parser::Parser BytecodeParser(Reader, ModuleBuilderDelegate);
  parser::customsections::Name Name;
  BytecodeParser.registerCustomSection(Name);

  try {
    BytecodeParser.parse();
  } catch (parser::ParserError const &Error) {
    panic(fmt::format("{}", Error.what()));
  }

  auto &BytecodeModule = ModuleBuilderDelegate.getModule();
  auto ValidationError = bytecode::validation::validate(BytecodeModule);
  try {
    if (ValidationError != nullptr) ValidationError->signal();
  } catch (bytecode::validation::MalformedError const &Error) {
    panic(fmt::format("{}", Error));
  } catch (bytecode::validation::TypeError const &Error) {
    panic(fmt::format("{}", Error));
  }
}

int main(int, char const *argv[]) {
  validate(argv[1]);
  return 0;
}