#include "bytecode/Module.h"
#include "bytecode/Validation.h"
#include "parser/ByteArrayReader.h"
#include "parser/ModuleBuilderDelegate.h"
#include "parser/Parser.h"

#include <mio/mmap.hpp>

int main(int argc, char const *argv[]) {
  utility::ignore(argc, argv);
  mio::basic_mmap_source<std::byte> Source("../test/viu.wasm");
  parser::ByteArrayReader Reader(Source);
  parser::ModuleBuilderDelegate D;
  parser::Parser Parser(Reader, D);
  Parser.parse();

  auto &Module = D.getModule();
  fmt::print("{}\n", Module.Imports.size());
  fmt::print("{}\n", Module.Functions[9].Body.size());

  using namespace bytecode::validation;
  try {
    validateThrow(std::addressof(Module));
  } catch (TypeError const &Error) {
    fmt::print("Type Error\n");
    // fmt::print("{}\n", Error.getLatestInstSite()->asPointer()->getOpcode());
    for (auto const &ValueType : Error.expecting())
      fmt::print("{} ", ValueType);
    fmt::print("\n");
    for (auto const &ValueType : Error.actual()) fmt::print("{} ", ValueType);
    fmt::print("\n");
  } catch (ValidationError const &Error) { fmt::print("Validation Error\n"); }
}
