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
  Instruction *I0 = BB.BuildInst<Constant>(std::int32_t(1));
  auto *I1 = BB.BuildInst<Constant>(std::int32_t(2));
  auto *I3 = BB.BuildInst<IntBinaryOp>(IntBinaryOperator::Add, I0, I1);
  fmt::print("{}\n", fmt::ptr(I0->getNextNode()));
  (void)I0;
  (void)I1;
  (void)I3;

  Function F(nullptr, bytecode::FunctionType({}, {bytecode::valuetypes::I32}));
  auto *L0 = F.BuildLocal(bytecode::valuetypes::F32);
  F.BuildLocal(bytecode::valuetypes::F64, L0);

  for (auto const &L : F.getLocals()) {
    fmt::print("{}  {}\n", L.getType(), L.isParameter());
  }
}
