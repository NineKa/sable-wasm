
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
  auto *I1 = BB.append<Constant>(10);
  auto *I2 = BB.append<Constant>(10);
  (void)I1;
  auto *I = BB.append<IntBinaryOp>(IntBinaryOperator::Add, nullptr, I2);

  for (auto *Inst : I->getOperands()) { fmt::print("{}\n", fmt::ptr(Inst)); }
  for (auto *Inst : I1->getUsedSites())
    fmt::print("I1 Site: {}\n", fmt::ptr(Inst));
  for (auto *Inst : I2->getUsedSites())
    fmt::print("I2 Site: {}\n", fmt::ptr(Inst));
}
