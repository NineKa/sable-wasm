#include "MIRPrinter.h"

#include <fmt/format.h>

namespace mir::printer {
ASTNodeNameResolver::ASTNodeNameResolver(Module const &M) {
  prepareMemories(M);
}

template <ranges::input_range T, typename NameGenFunctor>
void ASTNodeNameResolver::prepareEntities(T Entities, NameGenFunctor NameGen) {
  std::size_t UnknownCount = 0;
  for (ASTNode const &Entity : Entities) {
    if (Entity.hasName()) {
      auto DuplicateNameIter = UsedNames.find(Entity.getName());
      if (DuplicateNameIter != UsedNames.end()) {
        std::get<1>(*DuplicateNameIter) = std::get<1>(*DuplicateNameIter) + 1;
        auto Name = fmt::format(
            "{}{}", Entity.getName(), std::get<1>(*DuplicateNameIter));
        Storage.push_back(std::move(Name));
        Names.emplace(std::addressof(Entity), Storage.back());
      } else {
        UsedNames.emplace(Entity.getName(), 0);
        Names.emplace(std::addressof(Entity), Entity.getName());
      }
    } else {
      auto GenName = NameGen(UnknownCount);
      UnknownCount = UnknownCount + 1;
      Storage.push_back(std::move(GenName));
      Names.emplace(std::addressof(Entity), Storage.back());
    }
  }
}

namespace {
struct NameGenerator {
  char const *FormatStr;
  NameGenerator(char const *FormatStr_) : FormatStr(FormatStr_) {}
  std::string operator()(std::size_t Index) {
    return fmt::format(FormatStr, Index);
  }
};
} // namespace

void ASTNodeNameResolver::prepareMemories(Module const &M) {
  prepareEntities(M.getMemories(), NameGenerator("mem_{}"));
}

void ASTNodeNameResolver::prepareTables(Module const &M) {
  prepareEntities(M.getTables(), NameGenerator("table_{}"));
}

void ASTNodeNameResolver::prepareGlobals(Module const &M) {
  prepareEntities(M.getFunctions(), NameGenerator("global_{}"));
}

} // namespace mir::printer
