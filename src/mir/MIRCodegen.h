#ifndef SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN
#define SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN

#include "../bytecode/Module.h"
#include "../parser/customsections/Name.h"
#include "BasicBlock.h"
#include "Function.h"
#include "Instruction.h"
#include "Module.h"

#include <range/v3/view/zip.hpp>

#include <memory>
#include <vector>

namespace mir::bytecode_codegen {
class EntityLayout {
  bytecode::ModuleView BModuleView;
  std::vector<Function *> Functions;
  std::vector<Memory *> Memories;
  std::vector<Global *> Globals;
  std::vector<Table *> Tables;
  std::vector<Data *> DataSegments;
  std::vector<Element *> ElementSegments;

  std::unique_ptr<InitializerExpr>
  solveInitializerExpr(bytecode::Expression const &Expr);

public:
  EntityLayout(bytecode::Module const &BModule, Module &MModule);

  Function *operator[](bytecode::FuncIDX Index) const;
  Memory *operator[](bytecode::MemIDX Index) const;
  Table *operator[](bytecode::TableIDX Index) const;
  Global *operator[](bytecode::GlobalIDX Index) const;
  bytecode::FunctionType const *operator[](bytecode::TypeIDX Index) const;

  Memory *getImplicitMemory() const;
  Table *getImplicitTable() const;

  // clang-format off
  auto functions() const
  { return ranges::views::zip(BModuleView.functions(), Functions); }
  auto memories() const
  { return ranges::views::zip(BModuleView.memories(), Memories); }
  auto tables() const
  { return ranges::views::zip(BModuleView.tables(), Tables); }
  auto globals() const
  { return ranges::views::zip(BModuleView.globals(), Globals); }
  // clang-format on
};

class FunctionTranslationTask {
  class TranslationContext;
  class TranslationVisitor;
  std::unique_ptr<TranslationContext> Context;

public:
  FunctionTranslationTask(
      EntityLayout const &EntityLayout_,
      bytecode::views::Function SourceFunction_,
      mir::Function &TargetFunction_);
  FunctionTranslationTask(FunctionTranslationTask const &) = delete;
  FunctionTranslationTask(FunctionTranslationTask &&) noexcept;
  FunctionTranslationTask &operator=(FunctionTranslationTask const &) = delete;
  FunctionTranslationTask &operator=(FunctionTranslationTask &&) noexcept;
  ~FunctionTranslationTask() noexcept;

  void perform();
};

class ModuleTranslationTask {
  std::unique_ptr<EntityLayout> LayOut;
  bytecode::Module const *Source;
  mir::Module *Target;
  parser::customsections::Name const *Names;

public:
  ModuleTranslationTask(bytecode::Module const &Source_, mir::Module &Target_);
  ModuleTranslationTask(
      bytecode::Module const &Source_, mir::Module &Target_,
      parser::customsections::Name const &Names_);

  void perform();
};
} // namespace mir::bytecode_codegen

#endif
