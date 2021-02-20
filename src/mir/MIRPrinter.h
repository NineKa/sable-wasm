#ifndef SABLE_INCLUDE_GUARD_MIR_PRINTER
#define SABLE_INCLUDE_GUARD_MIR_PRINTER

#include "Instruction.h"
#include "Module.h"

#include <fmt/format.h>

#include <iterator>
#include <unordered_map>

namespace mir::printer {

template <std::output_iterator<char> Iterator>
Iterator indent(Iterator Out, std::size_t Width = 1) {
  char const IndentChar = ' ';
  std::size_t const IndentWidth = 2;
  for (std::size_t I = 0; I < IndentWidth * Width; ++I) *Out++ = IndentChar;
  return Out;
}

class EntityNameResolver {
  std::unordered_map<ASTNode const *, std::size_t> Names;

  template <ranges::input_range T>
  void prepareEntities(T Entities, std::size_t &Counter) {
    for (auto const &Entity : Entities) {
      if (Entity.hasName()) continue;
      Names.emplace(std::addressof(Entity), Counter);
      Counter = Counter + 1;
    }
  }

public:
  EntityNameResolver(Module const &Module_) {
    std::size_t UnnamedMemory = 0;
    std::size_t UnnamedTable = 0;
    std::size_t UnnamedGlobal = 0;
    std::size_t UnnamedFunction = 0;
    prepareEntities(Module_.getMemories(), UnnamedMemory);
    prepareEntities(Module_.getTables(), UnnamedTable);
    prepareEntities(Module_.getGlobals(), UnnamedGlobal);
    prepareEntities(Module_.getFunctions(), UnnamedFunction);
  }

  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, ASTNode const &EntityNode) {
    if (EntityNode.hasName())
      return fmt::format_to(Out, "%{}", EntityNode.getName());
    auto SearchIter = Names.find(std::addressof(EntityNode));
    assert(SearchIter != Names.end());
    auto UnnamedIndex = std::get<1>(*SearchIter);
    switch (EntityNode.getASTNodeKind()) {
    case ASTNodeKind::Memory:
      return fmt::format_to(Out, "%memory:{}", UnnamedIndex);
    case ASTNodeKind::Table:
      return fmt::format_to(Out, "%table:{}", UnnamedIndex);
    case ASTNodeKind::Global:
      return fmt::format_to(Out, "%global:{}", UnnamedIndex);
    case ASTNodeKind::Function:
      return fmt::format_to(Out, "%function:{}", UnnamedIndex);
    default: SABLE_UNREACHABLE();
    }
  }
};

struct EntityPrintPolicyBase {
  template <std::output_iterator<char> Iterator>
  void writeImportInfo(Iterator Out, detail::ImportableEntity const &Entity) {
    assert(Entity.isImported());
    auto ModuleName = Entity.getImportModuleName();
    auto EntityName = Entity.getImportEntityName();
    return fmt::format_to(Out, "import from {}::{}", ModuleName, EntityName);
  }
  template <std::output_iterator<char> Iterator>
  void writeExportInfo(Iterator Out, detail::ExportableEntity const &Entity) {
    assert(Entity.isExported());
    auto EntityName = Entity.getExportName();
    return fmt::format_to(Out, "export as {}", EntityName);
  }
};

} // namespace mir::printer

#endif
