#include "MIRCodegen.h"

#include <range/v3/view/transform.hpp>

namespace mir::bytecode_codegen {
ModuleTranslator::ModuleTranslator(
    bytecode::Module const &BModule_, mir::Module &MIRModule_)
    : BModule(BModule_), MIRModule(MIRModule_) {
  bytecode::ModuleView MView(BModule);
  // clang-format off
  Functions = MView.functions()
    | ranges::views::transform(
      [&](bytecode::views::Function const &FunctionView) {
        auto *MIRFunction = MIRModule.BuildFunction(*FunctionView.getType());
        return std::make_pair(MIRFunction, FunctionView);
      })
    | ranges::to<decltype(Functions)>();
  Globals = MView.globals()
    | ranges::views::transform(
      [&](bytecode::views::Global const &GlobalView) {
        auto *MIRGlobal = MIRModule.BuildGlobal(*GlobalView.getType());
        return std::make_pair(MIRGlobal, GlobalView);
      })
    | ranges::to<decltype(Globals)>();
  Memories = MView.memories()
    | ranges::views::transform(
      [&](bytecode::views::Memory const &MemoryView) {
        auto *MIRMemory = MIRModule.BuildMemory(*MemoryView.getType());
        return std::make_pair(MIRMemory, MemoryView);
      })
    | ranges::to<decltype(Memories)>();
  Tables = MView.tables()
    | ranges::views::transform(
      [&](bytecode::views::Table const &TableView) {
        auto *MIRTable = MIRModule.BuildTable(*TableView.getType());
        return std::make_pair(MIRTable, TableView);
      })
    | ranges::to<decltype(Tables)>();
  // clang-format on
}

Function *ModuleTranslator::getMIREntity(bytecode::FuncIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Functions.size());
  return std::get<0>(Functions[CastedIndex]);
}

Global *ModuleTranslator::getMIREntity(bytecode::GlobalIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Globals.size());
  return std::get<0>(Globals[CastedIndex]);
}

Memory *ModuleTranslator::getMIREntity(bytecode::MemIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Memories.size());
  return std::get<0>(Memories[CastedIndex]);
}

Table *ModuleTranslator::getMIREntity(bytecode::TableIDX Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < Tables.size());
  return std::get<0>(Tables[CastedIndex]);
}
} // namespace mir::bytecode_codegen