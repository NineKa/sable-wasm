
#include "mir/Instruction.h"
#include <fmt/format.h>

#include "bytecode/Validation.h"
#include "parser/ByteArrayReader.h"
#include "parser/ModuleBuilderDelegate.h"
#include "parser/Parser.h"

#include <mio/mmap.hpp>

int main(int argc, char const *argv[]) {
  (void)argc;
  (void)argv;

  mio::basic_mmap_source<std::byte> Source("../test/viu.wasm");
  parser::ByteArrayReader Reader(Source);
  parser::ModuleBuilderDelegate Delegate;
  parser::Parser Parser(Reader, Delegate);
  auto &Module = Delegate.getModule();
  if (auto Error = bytecode::validation::validate(Module)) Error->signal();

  using namespace mir;
  using namespace mir::instructions;

  Constant C(nullptr, 10);
  fmt::print("{}\n", C.getType());
}
