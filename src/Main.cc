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

  mio::basic_mmap_source<std::byte> Source("../test/main.wasm");
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

  bytecode::ModuleView ModuleView(Module);

  using namespace mir;
  using namespace mir::instructions;
  using namespace bytecode::valuetypes;
  using namespace mir::bytecode_codegen;

  mir::Module M;
  EntityMap EMap(Module, M);
  auto Iter = M.function_begin();
  TranslationTask Task(EMap, ModuleView.functions()[0], *Iter);
  Task.perform();

  std::ostream_iterator<char> OutIter(std::cout);
  mir::dump(OutIter, M);
  return 0;
}
