#include "bytecode/Validation.h"
#include "codegen-llvm-instance/LLVMCodege.h"
#include "mir/MIRCodegen.h"
#include "mir/MIRPrinter.h"
#include "mir/Module.h"
#include "parser/ByteArrayReader.h"
#include "parser/ModuleBuilderDelegate.h"
#include "parser/Parser.h"

#include <cxxopts.hpp>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <mio/mmap.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>

static cxxopts::ParseResult ArgOptions;

[[noreturn]] void panic(std::string_view Message) {
  fmt::print("{}\n", Message);
  std::exit(1);
}

void writeMIRModuleToFile(
    mir::Module const &Module, std::filesystem::path const &Out) {
  std::ofstream MIRFileOStream(Out);
  std::ostream_iterator<char> MIRFileIterator(MIRFileOStream);
  mir::dump(MIRFileIterator, Module);
  MIRFileOStream.close();
}

std::string getHostCPUFeatureString() {
  llvm::StringMap<bool> FeatureMap;
  llvm::sys::getHostCPUFeatures(FeatureMap);
  std::string Result;
  char const *Separator = "";
  for (auto const &FeaturePair : FeatureMap) {
    auto Feature = FeaturePair.first();
    auto HasFeature = FeaturePair.second;
    if (HasFeature) {
      Result.append(fmt::format("{}+{}", Separator, Feature));
      Separator = ",";
    }
  }
  return Result;
}

std::unique_ptr<llvm::TargetMachine> getNativeTargetMachine() {
  std::string Error;
  auto const &TargetTriplet = llvm::sys::getDefaultTargetTriple();
  auto CPUName = llvm::sys::getHostCPUName();
  auto CPUFeatureString = getHostCPUFeatureString();
  auto const *Target = llvm::TargetRegistry::lookupTarget(TargetTriplet, Error);
  if (!Target) panic(Error);
  auto RelocModel = llvm::Reloc::PIC_;
  auto CodeModel = llvm::CodeModel::Small;
  auto CodegenOptLevel = ArgOptions["opt"].as<bool>()
                             ? llvm::CodeGenOpt::Aggressive
                             : llvm::CodeGenOpt::None;
  llvm::TargetOptions Op;
  auto TargetMachine =
      std::unique_ptr<llvm::TargetMachine>(Target->createTargetMachine(
          TargetTriplet, CPUName, CPUFeatureString, Op, RelocModel, CodeModel,
          CodegenOptLevel));

  auto DataLayoutString =
      TargetMachine->createDataLayout().getStringRepresentation();
  fmt::print("Target CPU Name: {}\n", CPUName);
  fmt::print("Target CPU Features:\n{}\n", CPUFeatureString);
  fmt::print("Target Triplet : {}\n", TargetTriplet);
  fmt::print("Data Layout    : {}\n", DataLayoutString);
  fmt::print("Optimization   : {}\n", ArgOptions["opt"].as<bool>());

  return TargetMachine;
}

codegen::llvm_instance::TranslationOptions getMIRToLLVMCodegenOptions() {
  // clang-format off
  codegen::llvm_instance::TranslationOptions TOptions{
    .SkipMemBoundaryCheck =
      ArgOptions["codegen-no-memguard"].as<bool>() ||
      ArgOptions["unsafe"].as<bool>(),
    .SkipTblBoundaryCheck =
      ArgOptions["codegen-no-tblguard"].as<bool>() ||
      ArgOptions["unsafe"].as<bool>(),
    .AssumeMemRWAligned =
      ArgOptions["codegen-rw-aligned"].as<bool>()  ||
      ArgOptions["unsafe"].as<bool>()};
  // clang-format on
  return TOptions;
}

void optimizeLLVMModule(llvm::TargetMachine &TM, llvm::Module &Module) {
  auto const &TargetTriplet = TM.getTargetTriple();
  auto const &TargetIRAnalysis = TM.getTargetIRAnalysis();

  llvm::legacy::PassManager PM;
  PM.add(new llvm::TargetLibraryInfoWrapperPass(TargetTriplet));
  PM.add(llvm::createTargetTransformInfoWrapperPass(TargetIRAnalysis));

  llvm::legacy::FunctionPassManager FnPM(&Module);
  FnPM.add(llvm::createTargetTransformInfoWrapperPass(TargetIRAnalysis));

  llvm::PassManagerBuilder PassManagerBuilder;
  PassManagerBuilder.VerifyInput = true;
  PassManagerBuilder.VerifyOutput = true;
  PassManagerBuilder.OptLevel = 3;
  PassManagerBuilder.SizeLevel = 0;
  PassManagerBuilder.Inliner = llvm::createFunctionInliningPass(3, 0, false);
  PassManagerBuilder.LoopsInterleaved = true;
  PassManagerBuilder.LoopVectorize = true;
  PassManagerBuilder.SLPVectorize = true;
  TM.adjustPassManager(PassManagerBuilder);

  PassManagerBuilder.populateFunctionPassManager(FnPM);
  PassManagerBuilder.populateModulePassManager(PM);

  FnPM.doInitialization();
  for (llvm::Function &Function : Module) FnPM.run(Function);
  FnPM.doFinalization();

  PM.run(Module);
}

void process(
    std::filesystem::path const &In, std::filesystem::path const &Out) {
  if (!(std::filesystem::exists(In) && std::filesystem::is_regular_file(In)))
    panic(fmt::format("cannot locate {}", Out.c_str()));
  mio::basic_mmap_source<std::byte> Source(In.c_str());

  parser::ByteArrayReader Reader(Source);
  parser::ModuleBuilderDelegate ModuleBuilderDelegate;
  parser::Parser BytecodeParser(Reader, ModuleBuilderDelegate);
  parser::customsections::Name Name;
  BytecodeParser.registerCustomSection(Name);

  try {
    BytecodeParser.parse();
  } catch (parser::ParserError const &Error) {
    panic(fmt::format("{}", Error.what()));
  }

  auto &BytecodeModule = ModuleBuilderDelegate.getModule();

  auto ValidationError = bytecode::validation::validate(BytecodeModule);
  try {
    if (ValidationError != nullptr) ValidationError->signal();
  } catch (bytecode::validation::MalformedError const &Error) {
    panic(fmt::format("{}", Error));
  } catch (bytecode::validation::TypeError const &Error) {
    panic(fmt::format("{}", Error));
  }

  mir::Module MIRModule;
  mir::bytecode_codegen::ModuleTranslationTask BytecodeToMIRTranslationTask(
      BytecodeModule, MIRModule, Name);
  BytecodeToMIRTranslationTask.perform();

  if (ArgOptions["emit-mir"].as<bool>() || ArgOptions["debug"].as<bool>()) {
    auto MIRFilePath = Out;
    MIRFilePath.replace_extension(".mir");
    writeMIRModuleToFile(MIRModule, MIRFilePath);
  }

  auto TargetMachine = getNativeTargetMachine();
  llvm::LLVMContext LLVMContext;
  llvm::Module LLVMModule(In.stem().c_str(), LLVMContext);
  LLVMModule.setSourceFileName(In.c_str());
  LLVMModule.setCodeModel(llvm::CodeModel::Small);
  LLVMModule.setPICLevel(llvm::PICLevel::SmallPIC);
  LLVMModule.setDataLayout(TargetMachine->createDataLayout());
  LLVMModule.setTargetTriple(llvm::sys::getDefaultTargetTriple());

  auto MIRToLLVMTranslationOptions = getMIRToLLVMCodegenOptions();
  codegen::llvm_instance::ModuleTranslationTask MIRToLLVMTranslationTask(
      MIRModule, LLVMModule, MIRToLLVMTranslationOptions);
  MIRToLLVMTranslationTask.perform();

  if (ArgOptions["opt"].as<bool>())
    optimizeLLVMModule(*TargetMachine, LLVMModule);

  std::error_code ErrorCode;
  if (ArgOptions["emit-llvm"].as<bool>() || ArgOptions["debug"].as<bool>()) {
    auto LLVMFile = Out;
    LLVMFile.replace_extension(".ll");
    llvm::raw_fd_ostream LLOstream(LLVMFile.c_str(), ErrorCode);
    if (ErrorCode) panic(ErrorCode.message());
    LLVMModule.print(LLOstream, nullptr);
    LLOstream.flush();
  }

  llvm::raw_fd_ostream EmitOStream(Out.c_str(), ErrorCode);
  if (ErrorCode) panic(ErrorCode.message());
  llvm::legacy::PassManager PassManager;
  auto ObjectFileType = llvm::CGFT_ObjectFile;
  if (TargetMachine->addPassesToEmitFile(
          PassManager, EmitOStream, nullptr, ObjectFileType))
    panic("target machine cannot emit object file");
  PassManager.run(LLVMModule);
  EmitOStream.flush();
}

int main(int argc, char const *argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetDisassembler();

  cxxopts::Options Options("sable-wasm", "A static compiler for WebAssembly");
  // clang-format off
  Options.add_options()
  ("o,out"                , "output file name"                                 ,
   cxxopts::value<std::string>()->default_value("a.out"))
  ("opt"                  , "run optimization passes"                          ,
   cxxopts::value<bool>()->default_value("false"))
  ("emit-mir"             , "emit Sable middle IR (*.mir)"                     ,
   cxxopts::value<bool>()->default_value("false"))
  ("emit-llvm"            , "emit LLVM bytecode (*.ll)"                        ,
   cxxopts::value<bool>()->default_value("false"))
  ("d,debug"              , "debug mode"                                       ,
   cxxopts::value<bool>()->default_value("false"))
  ("unsafe"               , "take a leap of faith"                             ,
   cxxopts::value<bool>()->default_value("false"))
  ("codegen-no-memguard"  , "skip linear memory boundary check"                ,
   cxxopts::value<bool>()->default_value("false"))
  ("codegen-no-tblguard"  , "skip indirect table boundary check"               ,
   cxxopts::value<bool>()->default_value("false"))
  ("codegen-rw-aligned"   , "assume linear memory access is always aligned"    ,
   cxxopts::value<bool>()->default_value("false"))
  ;
  // clang-format on

  try {
    ArgOptions = Options.parse(argc, argv);
    if (ArgOptions.unmatched().size() != 1) panic(Options.help());
    auto In = ArgOptions.unmatched()[0];
    auto Dest = ArgOptions["out"].as<std::string>();
    process(In, Dest);
  } catch (cxxopts::option_not_exists_exception const &) {
    panic(Options.help());
  }

  return 0;
}
