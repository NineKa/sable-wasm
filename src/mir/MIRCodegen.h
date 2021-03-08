#ifndef SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN
#define SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN

#include "../bytecode/Module.h"
#include "../parser/customsections/Name.h"
#include "Module.h"

#include <memory>
#include <vector>

namespace mir::bytecode_codegen {
class EntityLayout {
  bytecode::ModuleView BModuleView;
  std::vector<Function *> Functions;
  std::vector<Memory *> Memories;
  std::vector<Global *> Globals;
  std::vector<Table *> Tables;

public:
  EntityLayout(bytecode::Module const &BModule, Module &MModule);

  Function *operator[](bytecode::FuncIDX Index) const;
  Memory *operator[](bytecode::MemIDX Index) const;
  Table *operator[](bytecode::TableIDX Index) const;
  Global *operator[](bytecode::GlobalIDX Index) const;
  bytecode::FunctionType const *operator[](bytecode::TypeIDX Index) const;

  void annotate(parser::customsections::Name const &Name);
  Memory *getImplicitMemory() const;
  Table *getImplicitTable() const;
};

class TranslationTask {
  class TranslationContext;
  class TranslationVisitor;
  std::unique_ptr<TranslationContext> Context;

public:
  TranslationTask(
      EntityLayout const &EntityLayout_,
      bytecode::views::Function SourceFunction_,
      mir::Function &TargetFunction_);
  TranslationTask(TranslationTask const &) = delete;
  TranslationTask(TranslationTask &&) noexcept;
  TranslationTask &operator=(TranslationTask const &) = delete;
  TranslationTask &operator=(TranslationTask &&) noexcept;
  ~TranslationTask() noexcept;

  void perform();
};

} // namespace mir::bytecode_codegen

#endif
