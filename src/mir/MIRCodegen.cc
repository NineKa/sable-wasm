#include "MIRCodegen.h"

#include <range/v3/view/transform.hpp>

namespace mir::bytecode_codegen {

namespace {
template <typename Derived> struct BuildMIREntity {
  Module &MIRModule;
  BuildMIREntity(Module &MIRModule_) : MIRModule(MIRModule_) {}
  Derived &getDerived() { return static_cast<Derived &>(*this); }
  Derived const &getDerived() const {
    return static_cast<Derived const &>(*this);
  }

  template <typename T> auto *operator()(T const &EntityView) {
    auto *MIREntity = getDerived().Build(EntityView);
    if (EntityView.isImported()) {
      std::string ModuleName(EntityView.getImportModuleName());
      std::string EntityName(EntityView.getImportEntityName());
      MIREntity->setImport(std::move(ModuleName), std::move(EntityName));
    }
    if (EntityView.isExported()) {
      std::string EntityName(EntityView.getExportName());
      MIREntity->setExport(std::move(EntityName));
    }
    return MIREntity;
  }
};

struct BuildMIRFunction : BuildMIREntity<BuildMIRFunction> {
  using BuildMIREntity::BuildMIREntity;
  Function *Build(bytecode::views::Function const &FunctionView) {
    auto *Function = MIRModule.BuildFunction(*FunctionView.getType());
    if (FunctionView.isDefinition()) {
      for (auto const &Local : FunctionView.getLocals()) {
        Function->BuildLocal(Local);
      }
    }
    return Function;
  }
};

struct BuildMIRMemory : BuildMIREntity<BuildMIRMemory> {
  using BuildMIREntity::BuildMIREntity;
  Memory *Build(bytecode::views::Memory const &MemoryView) {
    return MIRModule.BuildMemory(*MemoryView.getType());
  }
};

struct BuildMIRTable : BuildMIREntity<BuildMIRTable> {
  using BuildMIREntity::BuildMIREntity;
  Table *Build(bytecode::views::Table const &TableView) {
    return MIRModule.BuildTable(*TableView.getType());
  }
};

struct BuildMIRGlobal : BuildMIREntity<BuildMIRGlobal> {
  using BuildMIREntity::BuildMIREntity;
  Global *Build(bytecode::views::Global const &GlobalView) {
    return MIRModule.BuildGlobal(*GlobalView.getType());
  }
};
} // namespace

ModuleTranslator::ModuleTranslator(
    bytecode::Module const &BModule_, mir::Module &MIRModule_)
    : BModule(BModule_), MIRModule(MIRModule_) {
  bytecode::ModuleView MView(BModule);
  // clang-format off
  Functions = MView.functions()
    | ranges::views::transform(BuildMIRFunction(MIRModule))
    | ranges::to<decltype(Functions)>();
  Globals = MView.globals()
    | ranges::views::transform(BuildMIRGlobal(MIRModule))
    | ranges::to<decltype(Globals)>();
  Memories = MView.memories()
    | ranges::views::transform(BuildMIRMemory(MIRModule))
    | ranges::to<decltype(Memories)>();
  Tables = MView.tables()
    | ranges::views::transform(BuildMIRTable(MIRModule))
    | ranges::to<decltype(Tables)>();
  // clang-format on
}

void ModuleTranslator::annotateWith(
    parser::customsections::Name const &NameSection) {
  for (auto const &FunctionNameEntry : NameSection.getFunctionNames()) {
    auto *FuncEntity = getMIREntity(FunctionNameEntry.FuncIndex);
    FuncEntity->setName(FunctionNameEntry.Name);
  }
  for (auto const &LocalNameEntry : NameSection.getLocalNames()) {
    auto *FuncEntity = getMIREntity(LocalNameEntry.FuncIndex);
    auto LocalIter = FuncEntity->local_begin();
    std::advance(
        LocalIter, static_cast<std::size_t>(LocalNameEntry.LocalIndex));
    (*LocalIter).setName(LocalNameEntry.Name);
  }
}

Function *ModuleTranslator::getMIREntity(bytecode::FuncIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Functions.size());
  return Functions[CastedIndex];
}

Global *ModuleTranslator::getMIREntity(bytecode::GlobalIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Globals.size());
  return Globals[CastedIndex];
}

Memory *ModuleTranslator::getMIREntity(bytecode::MemIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Memories.size());
  return Memories[CastedIndex];
}

Table *ModuleTranslator::getMIREntity(bytecode::TableIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Tables.size());
  return Tables[CastedIndex];
}
} // namespace mir::bytecode_codegen
