#ifndef SABLE_INCLUDE_GUARD_PARSER_MODULE_BUILDER_DELEGATE
#define SABLE_INCLUDE_GUARD_PARSER_MODULE_BUILDER_DELEGATE

#include "../bytecode/Module.h"
#include "ExprBuilderDelegate.h"

#include <range/v3/core.hpp>
#include <range/v3/range/conversion.hpp>

namespace parser {
class ModuleBuilderDelegate : public ExprBuilderDelegate {
  bytecode::Module Module;

public:
  bytecode::Module &getModule() { return Module; }
  bytecode::Module const &getModule() const { return Module; }

  void enterTypeSection(SizeType Size) { Module.Types.reserve(Size); }
  void onTypeSectionEntry(SizeType Index, bytecode::FunctionType Type) {
    utility::ignore(Index);
    Module.Types.push_back(std::move(Type));
  }

  void enterImportSection(SizeType Size) { Module.Imports.reserve(Size); }
  void onImportSectionEntry(
      SizeType Index, std::string_view ModuleName, std::string_view EntityName,
      bytecode::ImportDescriptor Descriptor) {
    utility::ignore(Index);
    Module.Imports.push_back(bytecode::entities::Import{
        .ModuleName = std::string(ModuleName),
        .EntityName = std::string(EntityName),
        .Descriptor = Descriptor});
  }

  void enterFunctionSection(SizeType Size) { Module.Functions.reserve(Size); }
  void onFunctionSectionEntry(SizeType Index, bytecode::TypeIDX Type) {
    utility::ignore(Index);
    Module.Functions.push_back(
        bytecode::entities::Function{.Type = Type, .Locals = {}, .Body = {}});
  }

  void enterTableSection(SizeType Size) { Module.Tables.reserve(Size); }
  void onTableSectionEntry(SizeType Index, bytecode::TableType Type) {
    utility::ignore(Index);
    Module.Tables.emplace_back(bytecode::entities::Table{.Type = Type});
  }

  void enterMemorySection(SizeType Size) { Module.Memories.reserve(Size); }
  void onMemorySectionEntry(SizeType Index, bytecode::MemoryType Type) {
    utility::ignore(Index);
    Module.Memories.emplace_back(bytecode::entities::Memory{.Type = Type});
  }

  void enterGlobalSection(SizeType Size) { Module.Globals.reserve(Size); }
  void onGlobalSectionEntry(SizeType Index, bytecode::GlobalType Type) {
    utility::ignore(Index);
    Module.Globals.push_back(bytecode::entities::Global{
        .Type = Type, .Initializer = std::move(this->getExpression())});
  }

  void enterExportSection(SizeType Size) { Module.Exports.reserve(Size); }
  void onExportSectionEntry(
      SizeType Index, std::string_view EntityName,
      bytecode::ExportDescriptor Descriptor) {
    utility::ignore(Index);
    Module.Exports.emplace_back(bytecode::entities::Export{
        .Name = std::string(EntityName), .Descriptor = Descriptor});
  }

  void onStartSectionEntry(bytecode::FuncIDX Start) { Module.Start = Start; }

  void enterElementSection(SizeType Size) { Module.Elements.reserve(Size); }
  template <ranges::input_range T>
  void onElementSectionEntry(
      SizeType Index, bytecode::TableIDX Table, T &&Initializer) {
    static_assert(
        std::convertible_to<ranges::value_type_t<T>, bytecode::FuncIDX>);
    utility::ignore(Index);
    using VectorT = std::vector<bytecode::FuncIDX>;
    Module.Elements.push_back(bytecode::entities::Element{
        .Table = Table,
        .Offset = std::move(this->getExpression()),
        .Initializer = ranges::to<VectorT>(Initializer)});
  }

  void enterCodeSection(SizeType Size) {
    utility::ignore(Size);
    assert(Module.Functions.size() == Size);
  }
  template <ranges::input_range T>
  void onCodeSectionLocal(SizeType Index, T &&Types) {
    assert(Index < Module.Functions.size());
    auto &Function = Module.Functions[Index];
    using VectorT = std::vector<bytecode::ValueType>;
    Function.Locals = ranges::to<VectorT>(Types);
  }
  void onCodeSectionEntry(SizeType Index) {
    assert(Index < Module.Functions.size());
    auto &Function = Module.Functions[Index];
    Function.Body = std::move(this->getExpression());
  }

  void enterDataSection(SizeType Size) { Module.Data.reserve(Size); }
  template <ranges::input_range T>
  void onDataSectionEntry(SizeType Index, bytecode::MemIDX Memory, T &&C) {
    utility::ignore(Index);
    using VectorT = std::vector<std::byte>;
    Module.Data.push_back(bytecode::entities::Data{
        .Memory = Memory,
        .Offset = std::move(this->getExpression()),
        .Initializer = ranges::to<VectorT>(C)});
  }
};
} // namespace parser

#endif
