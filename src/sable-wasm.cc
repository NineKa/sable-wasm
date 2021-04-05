#include "mir/MIRCodegen.h"
#include "mir/MIRPrinter.h"
#include "mir/Module.h"

#include "bytecode/Validation.h"
#include "parser/ByteArrayReader.h"
#include "parser/ModuleBuilderDelegate.h"
#include "parser/Parser.h"

#include "codegen-llvm-instance/LLVMCodege.h"
#include "mir/ASTNode.h"
#include "mir/passes/IsWellformed.h"
#include "mir/passes/SimplifyCFG.h"

#include <cxxopts.hpp>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <mio/mmap.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>

void writeMIRModuleToFile(mir::Module const &Module, char const *Path) {
  std::ofstream MIRFileOStream(Path);
  std::ostream_iterator<char> MIRFileIterator(MIRFileOStream);
  mir::dump(MIRFileIterator, Module);
  MIRFileOStream.close();
}

void replaceOrAddExtension(std::string &Path, char const *Extension) {
  auto LastDot = Path.find_last_of('.');
  if (LastDot != std::string::npos) Path.resize(LastDot);
  Path.push_back('.');
  Path.append(Extension);
}

void process(
    char const *Path, char const *Out, cxxopts::ParseResult const &Options) {
  if (!std::filesystem::exists(Path)) {
    fmt::print("sable-wasm: cannot locate {}.", Path);
    std::exit(1);
  }
  mio::basic_mmap_source<std::byte> Source(Path);

  parser::ByteArrayReader Reader(Source);
  parser::ModuleBuilderDelegate ModuleBuilderDelegate;
  parser::Parser BytecodeParser(Reader, ModuleBuilderDelegate);
  parser::customsections::Name Name;
  BytecodeParser.registerCustomSection(Name);
  BytecodeParser.parse();

  auto &BytecodeModule = ModuleBuilderDelegate.getModule();

  auto ValidationError = bytecode::validation::validate(BytecodeModule);

  mir::Module MIRModule;
  mir::bytecode_codegen::ModuleTranslationTask BytecodeToMIRTranslationTask(
      BytecodeModule, MIRModule, Name);
  BytecodeToMIRTranslationTask.perform();

  if (Options["emit-mir"].as<bool>() || Options["debug"].as<bool>()) {
    std::string MIRFile(Out);
    replaceOrAddExtension(MIRFile, "mir");
    writeMIRModuleToFile(MIRModule, MIRFile.c_str());
  }

  mir::passes::SimpleForEachFunctionPassDriver<mir::passes::SimplifyCFGPass>
      MIRSimplifyCFG;
  MIRSimplifyCFG(MIRModule);

  if (Options["emit-opt-mir"].as<bool>() || Options["debug"].as<bool>()) {
    std::string MIROptFile(Out);
    replaceOrAddExtension(MIROptFile, "opt.mir");
    writeMIRModuleToFile(MIRModule, MIROptFile.c_str());
  }

  llvm::LLVMContext LLVMContext;
  llvm::Module LLVMModule(Path, LLVMContext);
  std::string Error;
  auto const *Target = llvm::TargetRegistry::lookupTarget(
      llvm::sys::getDefaultTargetTriple(), Error);
  if (!Target) throw std::runtime_error(Error);
  auto RelocModel = llvm::Reloc::PIC_;
  llvm::TargetOptions Op;
  std::unique_ptr<llvm::TargetMachine> TargetMachine(
      Target->createTargetMachine(
          llvm::sys::getDefaultTargetTriple(), "generic", "", Op, RelocModel));
  LLVMModule.setDataLayout(TargetMachine->createDataLayout());
  LLVMModule.setTargetTriple(llvm::sys::getDefaultTargetTriple());

  codegen::llvm_instance::ModuleTranslationTask MIRToLLVMTranslationTask(
      MIRModule, LLVMModule);
  MIRToLLVMTranslationTask.perform();

  std::error_code ErrorCode;
  if (Options["emit-llvm"].as<bool>() || Options["debug"].as<bool>()) {
    std::string LLFile(Out);
    replaceOrAddExtension(LLFile, "ll");
    llvm::raw_fd_ostream LLOstream(LLFile, ErrorCode);
    if (ErrorCode) {
      fmt::print("sable-wasm: {}", ErrorCode.message());
      std::exit(1);
    }
    LLVMModule.print(LLOstream, nullptr);
    LLOstream.flush();
  }

  llvm::raw_fd_ostream ObjOstream(Out, ErrorCode);
  if (ErrorCode) {
    fmt::print("sable-wasm: {}", ErrorCode.message());
    std::exit(1);
  }
  llvm::legacy::PassManager PassManager;
  auto ObjectFileType = llvm::CGFT_ObjectFile;
  if (TargetMachine->addPassesToEmitFile(
          PassManager, ObjOstream, nullptr, ObjectFileType)) {
    fmt::print("sable-wasm: target machine cannot emit object file");
    std::exit(1);
  }
  PassManager.run(LLVMModule);
  ObjOstream.flush();
}

int main(int argc, char const *argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetDisassembler();

  cxxopts::Options Options("sable-wasm", "A static compiler for WebAssembly");
  // clang-format off
  Options.add_options()
  ("o,out"       , "output file name"                     , cxxopts::value<std::string>()->default_value("a.out"))
  ("emit-mir"    , "emit sable middle ir"                 , cxxopts::value<bool>()->default_value("false"))
  ("emit-opt-mir", "emit sable middle ir after all passes", cxxopts::value<bool>()->default_value("false"))
  ("emit-llvm"   , "emit llvm bytecode before codegen"    , cxxopts::value<bool>()->default_value("false"))
  ("d,debug"     , "debug mode"                           , cxxopts::value<bool>()->default_value("false"));
  // clang-format on

  auto OptionResult = Options.parse(argc, argv);
  if (OptionResult.unmatched().size() != 1) {
    fmt::print("{}\n", Options.help());
    std::exit(1);
  }

  process(
      OptionResult.unmatched()[0].c_str(),
      OptionResult["out"].as<std::string>().c_str(), OptionResult);

  return 0;
}
