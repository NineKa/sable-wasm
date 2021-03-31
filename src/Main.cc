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
#include "mir/passes/IsWellformed.h"
#include "mir/passes/SimplifyCFG.h"

#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>

#include <fstream>
#include <iostream>

int main(int argc, char const *argv[]) {
  (void)argc;
  (void)argv;

  mio::basic_mmap_source<std::byte> Source(
      //    "../test/polybench-c-4.2.1-beta/2mm.wasm");
      "../test/2mm.wasm");
  // "../test/main.wasm");
  // "../test/viu.wasm");

  using namespace std::chrono;
  parser::ByteArrayReader Reader(Source);
  parser::ModuleBuilderDelegate Delegate;
  parser::Parser Parser(Reader, Delegate);
  parser::customsections::Name Name;
  auto ParseTime = utility::measure([&]() {
    Parser.registerCustomSection(Name);
    Parser.parse();
  });
  fmt::print(
      "Bytecode Parsing: {} milliseconds\n",
      duration_cast<milliseconds>(ParseTime).count());

  auto &Module = Delegate.getModule();

  auto ValidationTime = utility::measure([&]() {
    try {
      if (auto Error = bytecode::validation::validate(Module)) Error->signal();
    } catch (bytecode::validation::TypeError const &Error) {
      fmt::print("{}\n", Error);
    } catch (bytecode::validation::MalformedError const &Error) {
      fmt::print("{}\n", Error);
    }
  });
  fmt::print(
      "Bytecode Validation: {} milliseconds\n",
      duration_cast<milliseconds>(ValidationTime).count());

  bytecode::ModuleView ModuleView(Module);

  using namespace mir;
  using namespace mir::instructions;
  using namespace bytecode::valuetypes;
  using namespace mir::bytecode_codegen;

  mir::Module M;

  auto TranslationTime = utility::measure([&]() {
    ModuleTranslationTask Task(Module, M, Name);
    Task.perform();
  });
  fmt::print(
      "MIR Translation: {} milliseconds\n",
      duration_cast<milliseconds>(TranslationTime).count());

  auto MIRValidationTime = utility::measure([&]() {
    if (!mir::validate(M)) utility::unreachable();
  });
  fmt::print(
      "MIR Validation Time: {} milliseconds\n",
      duration_cast<milliseconds>(MIRValidationTime).count());

  std::ofstream OutFile("out.mir");
  std::ostream_iterator<char> It(OutFile);
  mir::dump(It, M);

  auto SimplifyTime = utility::measure([&]() {
    mir::passes::SimpleForEachFunctionPassDriver<mir::passes::SimplifyCFGPass>
        Driver;
    Driver(M);

    for (auto const &Function : M.getFunctions().asView()) {
      if (Function.isDeclaration()) continue;
      mir::passes::SimpleFunctionPassDriver<mir::passes::DominatorPass> D;
      auto DomTree = D(Function).buildDomTree(Function.getEntryBasicBlock());
      utility::ignore(DomTree);
      // fmt::print(
      //    "{} : {}\n", Function.getName(), DomTree->getChildren().size());
    }
  });
  fmt::print(
      "MIR Simplification Time: {} milliseconds\n",
      duration_cast<milliseconds>(SimplifyTime).count());

  std::ofstream OutOptFile("out.opt.mir");
  std::ostream_iterator<char> It2(OutOptFile);
  mir::dump(It2, M);

  if (!mir::validate(M)) utility::unreachable();

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetDisassembler();

  using namespace codegen::llvm_instance;
  llvm::LLVMContext CTX;
  llvm::Module LM("module", CTX);
  std::string Error;
  auto const *Target = llvm::TargetRegistry::lookupTarget(
      llvm::sys::getDefaultTargetTriple(), Error);
  if (!Target) utility::unreachable();
  auto RM = llvm::Optional<llvm::Reloc::Model>();
  llvm::TargetOptions Op;
  std::unique_ptr<llvm::TargetMachine> TargetMachine(
      Target->createTargetMachine(
          llvm::sys::getDefaultTargetTriple(), "generic", "", Op, RM));
  LM.setDataLayout(TargetMachine->createDataLayout());
  LM.setTargetTriple(llvm::sys::getDefaultTargetTriple());

  auto LLVMCodegenTime = utility::measure([&]() {
    codegen::llvm_instance::EntityLayout ELayout(M, LM);
    // LM.print(llvm::outs(), nullptr);
  });
  fmt::print(
      "LLVM Codegen Time: {} milliseconds\n",
      duration_cast<milliseconds>(LLVMCodegenTime).count());
  return 0;
}
