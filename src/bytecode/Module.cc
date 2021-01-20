#include "Module.h"

namespace bytecode {
ModuleView::ModuleView(Module const &M_)
    : Storage(std::make_shared<ViewStorage>()) {
  Storage->M = std::addressof(M_);
  for (auto const &Import : Storage->M->Imports) {
    if (Import.Descriptor.is<TypeIDX>()) {
      views::Function FunctionView;
      FunctionView.Import = std::addressof(Import);
      FunctionView.Type = operator[](Import.Descriptor.as<TypeIDX>());
      Storage->Functions.push_back(FunctionView);
    } else if (Import.Descriptor.is<TableType>()) {
      views::Table TableView;
      TableView.Import = std::addressof(Import);
      TableView.Type = std::addressof(Import.Descriptor.as<TableType>());
      Storage->Tables.push_back(TableView);
    } else if (Import.Descriptor.is<MemoryType>()) {
      views::Memory MemoryView;
      MemoryView.Import = std::addressof(Import);
      MemoryView.Type = std::addressof(Import.Descriptor.as<MemoryType>());
      Storage->Memories.push_back(MemoryView);
    } else if (Import.Descriptor.is<GlobalType>()) {
      views::Global GlobalView;
      GlobalView.Import = std::addressof(Import);
      GlobalView.Type = std::addressof(Import.Descriptor.as<GlobalType>());
      Storage->Globals.push_back(GlobalView);
    } else
      SABLE_UNREACHABLE();
  }
  /* Entity Definition */
  for (auto const &Function : Storage->M->Functions) {
    views::Function FunctionView;
    FunctionView.Type = operator[](Function.Type);
    FunctionView.Entity = std::addressof(Function);
    Storage->Functions.push_back(FunctionView);
  }
  for (auto const &Table : Storage->M->Tables) {
    views::Table TableView;
    TableView.Type = std::addressof(Table.Type);
    Storage->Tables.push_back(TableView);
  }
  for (auto const &Memory : Storage->M->Memories) {
    views::Memory MemoryView;
    MemoryView.Type = std::addressof(Memory.Type);
    Storage->Memories.push_back(MemoryView);
  }
  for (auto const &Global : Storage->M->Globals) {
    views::Global GlobalView;
    GlobalView.Type = std::addressof(Global.Type);
    GlobalView.Entity = std::addressof(Global);
    Storage->Globals.push_back(GlobalView);
  }
  /* Annotating with Export */
  for (auto const &Export : Storage->M->Exports) {
    if (Export.Descriptor.is<FuncIDX>()) {
      auto CastedIndex =
          static_cast<std::size_t>(Export.Descriptor.as<FuncIDX>());
      assert(CastedIndex < Storage->Functions.size());
      auto &FunctionView = Storage->Functions[CastedIndex];
      FunctionView.Export = std::addressof(Export);
    } else if (Export.Descriptor.is<TableIDX>()) {
      auto CastedIndex =
          static_cast<std::size_t>(Export.Descriptor.as<TableIDX>());
      assert(CastedIndex < Storage->Tables.size());
      auto &TableView = Storage->Tables[CastedIndex];
      TableView.Export = std::addressof(Export);
    } else if (Export.Descriptor.is<MemIDX>()) {
      auto CastedIndex =
          static_cast<std::size_t>(Export.Descriptor.as<MemIDX>());
      assert(CastedIndex < Storage->Memories.size());
      auto &MemoryView = Storage->Memories[CastedIndex];
      MemoryView.Export = std::addressof(Export);
    } else if (Export.Descriptor.is<GlobalIDX>()) {
      auto CastedIndex =
          static_cast<std::size_t>(Export.Descriptor.as<GlobalIDX>());
      assert(CastedIndex < Storage->Globals.size());
      auto &GlobalView = Storage->Globals[CastedIndex];
      GlobalView.Export = std::addressof(Export);
    } else
      SABLE_UNREACHABLE();
  }
}

namespace {
template <ranges::random_access_range R, typename T>
ranges::range_value_t<R> const &getByIndex(R const &Range, T const &Index) {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < ranges::size(Range));
  return ranges::begin(Range)[CastedIndex];
}
} // namespace

// clang-format off
FunctionType const *ModuleView::operator[](TypeIDX const &Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < ranges::size(Storage->M->Types));
  return std::addressof(Storage->M->Types[CastedIndex]);
}
views::Table const &ModuleView::operator[](TableIDX const &Index) const
{ return getByIndex(Storage->Tables, Index); }
views::Memory const &ModuleView::operator[](MemIDX const &Index) const
{ return getByIndex(Storage->Memories, Index); }
views::Global const &ModuleView::operator[](GlobalIDX const &Index) const
{ return getByIndex(Storage->Globals, Index); }
views::Function const &ModuleView::operator[](FuncIDX const &Index) const
{ return getByIndex(Storage->Functions, Index); }
// clang-format on
} // namespace bytecode
