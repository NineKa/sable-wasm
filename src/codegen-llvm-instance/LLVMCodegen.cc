#include "LLVMCodegen.h"
#include "TranslationVisitor.h"

namespace codegen::llvm_instance {

FunctionTranslationTask::FunctionTranslationTask(
    EntityLayout &EntityLayout_, mir::Function const &Source_,
    llvm::Function &Target_)
    : Context(std::make_unique<TranslationContext>(
          EntityLayout_, Source_, Target_)) {}

FunctionTranslationTask::FunctionTranslationTask(
    FunctionTranslationTask &&) noexcept = default;
FunctionTranslationTask &FunctionTranslationTask::operator=(
    FunctionTranslationTask &&) noexcept = default;
FunctionTranslationTask::~FunctionTranslationTask() noexcept = default;

void FunctionTranslationTask::perform() {
  for (auto const *BBPtr : Context->getDominatorTree()->asPreorder()) {
    auto [FirstBBPtr, LastBBPtr] = Context->operator[](*BBPtr);
    utility::ignore(LastBBPtr);
    IRBuilder Builder(*FirstBBPtr);
    TranslationVisitor Visitor(*Context, Builder);
    for (auto const &Instruction : *BBPtr) {
      auto *Value = Visitor.visit(std::addressof(Instruction));
      Context->setValueMapping(Instruction, Value);
    }
  }

  for (auto const *BBPtr : Context->getDominatorTree()->asPreorder())
    for (auto const &Instruction : *BBPtr) {
      if (!mir::is_a<mir::instructions::Phi>(Instruction)) continue;
      auto &PhiNode = mir::dyn_cast<mir::instructions::Phi>(Instruction);
      auto *LLVMPhi =
          llvm::dyn_cast<llvm::PHINode>(Context->operator[](Instruction));
      for (auto [Value, Path] : PhiNode.getCandidates()) {
        auto *LLVMValue = Context->operator[](*Value);
        auto [LLVMFirstBB, LLVMLastBB] = Context->operator[](*Path);
        utility::ignore(LLVMFirstBB);
        LLVMPhi->addIncoming(LLVMValue, LLVMLastBB);
      }
    }
}

ModuleTranslationTask::ModuleTranslationTask(
    mir::Module const &Source_, llvm::Module &Target_,
    TranslationOptions Options_)
    : Layout(nullptr), Source(std::addressof(Source_)),
      Target(std::addressof(Target_)), Options(Options_) {}

void ModuleTranslationTask::perform() {
  Layout = std::make_unique<EntityLayout>(*Source, *Target, Options);
  for (auto const &Function : Source->getFunctions().asView()) {
    if (Function.isDeclaration()) continue;
    auto &TargetFunction = *Layout->operator[](Function).definition();
    FunctionTranslationTask Task(*Layout, Function, TargetFunction);
    Task.perform();
  }
}

} // namespace codegen::llvm_instance
