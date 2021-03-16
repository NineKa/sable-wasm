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

#include "codegen-llvm-instance/LLVMCodege.h"
#include "mir/ASTNode.h"

#include <iostream>

int main(int argc, char const *argv[]) {
  (void)argc;
  (void)argv;

  mio::basic_mmap_source<std::byte> Source(
      "../test/polybench-c-4.2.1-beta/2mm.wasm");
  //"../test/main.wasm");
  //"../test/viu.wasm");
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

  ModuleTranslationTask Task(Module, M, Name);
  Task.perform();

  // std::ostream_iterator<char> It(std::cout);
  // mir::dump(It, M);

  using namespace codegen::llvm_instance;
  llvm::LLVMContext CTX;
  llvm::Module LM("module", CTX);
  codegen::llvm_instance::EntityLayout ELayout(M, LM);
  LM.print(llvm::outs(), nullptr);
}
