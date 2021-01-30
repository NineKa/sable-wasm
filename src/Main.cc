
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
  Parser.parse();
  auto &Module = Delegate.getModule();
  try {
    if (auto Error = bytecode::validation::validate(Module)) Error->signal();
  } catch (bytecode::validation::TypeError const &Error) {
    fmt::print("{}\n", Error);
  } catch (bytecode::validation::MalformedError const &Error) {
    fmt::print("{}\n", Error);
  }

  using namespace mir;
  using namespace mir::instructions;

  BasicBlock BB;
  auto *Inst = BB.append<Branch>(&BB);
  fmt::print("{}\n", fmt::ptr(Inst));
}
