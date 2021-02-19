#ifndef SABLE_INCLUDE_GUARD_MIR_PRINTER
#define SABLE_INCLUDE_GUARD_MIR_PRINTER

#include "Module.h"

#include <iterator>
#include <type_traits>

namespace mir::printer {

// clang-format off
template <typename T> concept name_resolver = requires(T Resolver) {
  requires std::is_constructible_v<T, mir::Module>;
  { Resolver[std::declval<mir::Instruction *>()] }
  ->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::BasicBlock *>()] }
  ->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::Function *>()] }->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::Memory *>()]   }->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::Table *>()]    }->std::same_as<std::string_view>;
  { Resolver[std::declval<mir::Global *>()]   }->std::same_as<std::string_view>;
};
// clang-format on

} // namespace mir::printer

#endif
