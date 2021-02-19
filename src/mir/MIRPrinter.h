#ifndef SABLE_INCLUDE_GUARD_MIR_PRINTER
#define SABLE_INCLUDE_GUARD_MIR_PRINTER

#include "Instruction.h"
#include "Module.h"

#include <iterator>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mir::printer {

template <typename T> concept name_resolver = requires(T Resolver) {
  requires std::is_constructible_v<T, mir::Module>;
  { Resolver[std::declval<mir::Instruction const *>()] }
  ->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::BasicBlock const *>()] }
  ->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::Function const *>()] }
  ->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::Memory const *>()] }
  ->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::Table const *>()] }
  ->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::Global const *>()] }
  ->std::same_as<std::string_view>;
};

class ASTNodeNameResolver {
  std::unordered_map<ASTNode const *, std::string_view> Names;
  std::unordered_map<std::string_view, unsigned> UsedNames;
  std::vector<std::string> Storage;

  void prepareMemories(Module const &M);
  void prepareTable(Module const &M);
  void prepareGlobal(Module const &M);

  template <ranges::input_range T, typename NameGenFunctor>
  void prepareEntities(T Entities, NameGenFunctor NameGen);

public:
  ASTNodeNameResolver(Module const &M);

  std::string_view operator[](ASTNode const *ASTNode_);
};

} // namespace mir::printer

#endif
