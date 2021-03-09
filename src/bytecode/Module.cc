#include "Module.h"

namespace bytecode {
ModuleView::ModuleView(Module const &M_)
    : Storage(std::make_shared<ViewStorage>()) {
  Storage->M = std::addressof(M_);
  for (auto const &Import : Storage->M->Imports) {
    if (std::holds_alternative<TypeIDX>(Import.Descriptor)) {
      views::Function FunctionView;
      FunctionView.Import = std::addressof(Import);
      FunctionView.Type = operator[](std::get<TypeIDX>(Import.Descriptor));
      FunctionView.Index = static_cast<FuncIDX>(Storage->Functions.size());
      Storage->Functions.push_back(FunctionView);
    } else if (std::holds_alternative<TableType>(Import.Descriptor)) {
      views::Table TableView;
      TableView.Import = std::addressof(Import);
      TableView.Type = std::addressof(std::get<TableType>(Import.Descriptor));
      TableView.Index = static_cast<TableIDX>(Storage->Tables.size());
      Storage->Tables.push_back(TableView);
    } else if (std::holds_alternative<MemoryType>(Import.Descriptor)) {
      views::Memory MemoryView;
      MemoryView.Import = std::addressof(Import);
      MemoryView.Type = std::addressof(std::get<MemoryType>(Import.Descriptor));
      MemoryView.Index = static_cast<MemIDX>(Storage->Memories.size());
      Storage->Memories.push_back(MemoryView);
    } else if (std::holds_alternative<GlobalType>(Import.Descriptor)) {
      views::Global GlobalView;
      GlobalView.Import = std::addressof(Import);
      GlobalView.Type = std::addressof(std::get<GlobalType>(Import.Descriptor));
      GlobalView.Index = static_cast<GlobalIDX>(Storage->Globals.size());
      Storage->Globals.push_back(GlobalView);
    } else
      SABLE_UNREACHABLE();
  }
  /* Entity Definition */
  for (auto const &Function : Storage->M->Functions) {
    views::Function FunctionView;
    FunctionView.Type = operator[](Function.Type);
    FunctionView.Entity = std::addressof(Function);
    FunctionView.Index = static_cast<FuncIDX>(Storage->Functions.size());
    Storage->Functions.push_back(FunctionView);
  }
  for (auto const &Table : Storage->M->Tables) {
    views::Table TableView;
    TableView.Type = std::addressof(Table.Type);
    TableView.Index = static_cast<TableIDX>(Storage->Tables.size());
    Storage->Tables.push_back(TableView);
  }
  for (auto const &Memory : Storage->M->Memories) {
    views::Memory MemoryView;
    MemoryView.Type = std::addressof(Memory.Type);
    MemoryView.Index = static_cast<MemIDX>(Storage->Memories.size());
    Storage->Memories.push_back(MemoryView);
  }
  for (auto const &Global : Storage->M->Globals) {
    views::Global GlobalView;
    GlobalView.Type = std::addressof(Global.Type);
    GlobalView.Entity = std::addressof(Global);
    GlobalView.Index = static_cast<GlobalIDX>(Storage->Globals.size());
    Storage->Globals.push_back(GlobalView);
  }
  /* Annotating with Export */
  for (auto const &Export : Storage->M->Exports) {
    if (std::holds_alternative<FuncIDX>(Export.Descriptor)) {
      auto CastedIndex =
          static_cast<std::size_t>(std::get<FuncIDX>(Export.Descriptor));
      assert(CastedIndex < Storage->Functions.size());
      auto &FunctionView = Storage->Functions[CastedIndex];
      FunctionView.Export = std::addressof(Export);
    } else if (std::holds_alternative<TableIDX>(Export.Descriptor)) {
      auto CastedIndex =
          static_cast<std::size_t>(std::get<TableIDX>(Export.Descriptor));
      assert(CastedIndex < Storage->Tables.size());
      auto &TableView = Storage->Tables[CastedIndex];
      TableView.Export = std::addressof(Export);
    } else if (std::holds_alternative<MemIDX>(Export.Descriptor)) {
      auto CastedIndex =
          static_cast<std::size_t>(std::get<MemIDX>(Export.Descriptor));
      assert(CastedIndex < Storage->Memories.size());
      auto &MemoryView = Storage->Memories[CastedIndex];
      MemoryView.Export = std::addressof(Export);
    } else if (std::holds_alternative<GlobalIDX>(Export.Descriptor)) {
      auto CastedIndex =
          static_cast<std::size_t>(std::get<GlobalIDX>(Export.Descriptor));
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
  return *(ranges::begin(Range) + CastedIndex);
}
} // namespace

FunctionType const *ModuleView::operator[](TypeIDX const &Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  assert(CastedIndex < ranges::size(Storage->M->Types));
  return std::addressof(Storage->M->Types[CastedIndex]);
}
// clang-format off
views::Table const &ModuleView::operator[](TableIDX const &Index) const
{ return getByIndex(Storage->Tables, Index); }
views::Memory const &ModuleView::operator[](MemIDX const &Index) const
{ return getByIndex(Storage->Memories, Index); }
views::Global const &ModuleView::operator[](GlobalIDX const &Index) const
{ return getByIndex(Storage->Globals, Index); }
views::Function const &ModuleView::operator[](FuncIDX const &Index) const
{ return getByIndex(Storage->Functions, Index); }
// clang-format on

namespace {
template <ranges::random_access_range R, typename T>
std::optional<ranges::range_value_t<R>>
getByIndexOptional(R const &Range, T const &Index) {
  if (!(static_cast<std::size_t>(Index) < ranges::size(Range)))
    return std::nullopt;
  return getByIndex(Range, Index);
}
} // namespace

std::optional<FunctionType const *>
ModuleView::get(TypeIDX const &Index) const {
  if (!(static_cast<std::size_t>(Index) < Storage->M->Types.size()))
    return std::nullopt;
  return operator[](Index);
}
// clang-format off
std::optional<views::Table> ModuleView::get(TableIDX const &Index) const
{ return getByIndexOptional(Storage->Tables, Index); }
std::optional<views::Memory> ModuleView::get(MemIDX const &Index) const 
{ return getByIndexOptional(Storage->Memories, Index); }
std::optional<views::Global> ModuleView::get(GlobalIDX const &Index) const 
{ return getByIndexOptional(Storage->Globals, Index); }
std::optional<views::Function> ModuleView::get(FuncIDX const &Index) const
{ return getByIndexOptional(Storage->Functions, Index); }
// clang-format on

} // namespace bytecode
