#include "mir/Instruction.h"
#include "mir/MIRCodegen.h"
#include "mir/MIRPrinter.h"
#include "mir/Module.h"
#include <fmt/format.h>

#include "bytecode/Validation.h"
#include "parser/ByteArrayReader.h"
#include "parser/ModuleBuilderDelegate.h"
#include "parser/Parser.h"

#include <mio/mmap.hpp>

#include "mir/ASTNode.h"

#include <iostream>

int main(int argc, char const *argv[]) {
  (void)argc;
  (void)argv;

  mio::basic_mmap_source<std::byte> Source(
      "../test/polybench-c-4.2.1-beta/2mm.wasm");
  parser::ByteArrayReader Reader(Source);
  parser::ModuleBuilderDelegate Delegate;
  parser::Parser Parser(Reader, Delegate);
  parser::customsections::Name Name;
  Parser.registerCustomSection(Name);
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
  using namespace bytecode::valuetypes;
  using namespace mir::bytecode_codegen;

  mir::Module M;

  auto *Function = M.BuildFunction(bytecode::FunctionType({}, {I32}));
  auto *BB = Function->BuildBasicBlock();
  auto *I0 = BB->BuildInst<Constant>(std::int32_t(0));
  auto *I1 = BB->BuildInst<Constant>(std::int32_t(1));
  auto *I2 = BB->BuildInst<IntBinaryOp>(IntBinaryOperator::Add, I0, I1);
  BB->BuildInst<Return>(I2);

  mir::printer::ASTNodeNameResolver Resolver(M);
  std::ostream_iterator<char> OutIter(std::cout);
  mir::printer::FunctionPrintPolicy Policy(Resolver);

  for (auto const &F : M.getFunctions()) {
    Resolver.enter(F);
    Policy(OutIter, F);
    *OutIter = '\n';
  }
}
