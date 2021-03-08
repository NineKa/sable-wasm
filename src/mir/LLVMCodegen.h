#ifndef SABLE_INCLUDE_GUARD_LLVM_CODEGEN
#define SABLE_INCLUDE_GUARD_LLVM_CODEGEN

#include "../bytecode/Module.h"
#include "Module.h"

#include <llvm/IR/Module.h>

#include <memory>

namespace mir::llvm_codegen {
class TranslationTask {
  class TranslationContext;
  class TranslationVisitor;

  std::unique_ptr<TranslationContext> Context;
  std::unique_ptr<TranslationVisitor> Visitor;

public:
  TranslationTask(
      bytecode::ModuleView BytecodeModule, mir::Module const &Source,
      llvm::Module &Target);
  TranslationTask(TranslationTask const &) = delete;
  TranslationTask(TranslationTask &&) noexcept;
  TranslationTask &operator=(TranslationTask const &) = delete;
  TranslationTask &operator=(TranslationTask &&) noexcept;
  ~TranslationTask() noexcept;

  void perform();
};
} // namespace mir::llvm_codegen

#endif
