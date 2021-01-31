#include "mir/Instruction.h"
#include "mir/MIRPrinter.h"
#include "mir/Module.h"
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

  BasicBlock BB(nullptr);
  auto *I0 = BB.append<Constant>(std::int32_t(1));
  auto *I1 = BB.append<Constant>(std::int32_t(2));
  auto *I3 = BB.append<IntBinaryOp>(IntBinaryOperator::Add, I0, I1);
  fmt::print("{}\n", I3->getLHS() == nullptr);
  BB.erase(I0);
  fmt::print("{}\n", I3->getLHS() == nullptr);
}
