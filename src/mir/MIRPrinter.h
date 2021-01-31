#ifndef SABLE_INCLUDE_GUARD_MIR_PRINTER
#define SABLE_INCLUDE_GUARD_MIR_PRINTER

#include "Module.h"

#include <iterator>

namespace mir::printer {
template <typename T> concept entity_name_map = requires(T NameMap) {
  { NameMap[std::declval<Function const *>()] }
  ->std::same_as<std::string_view>;
  { NameMap[std::declval<Memory const *>()] }
  ->std::same_as<std::string_view>;
  { NameMap[std::declval<Table const *>()] }
  ->std::same_as<std::string_view>;
  { NameMap[std::declval<Global const *>()] }
  ->std::same_as<std::string_view>;
};

template <typename T> concept local_name_map = requires(T NameMap) {
  { NameMap[std::declval<Local const *>()] }
  ->std::same_as<std::string_view>;
  { NameMap[std::declval<Instruction const *>()] }
  ->std::same_as<std::string_view>;
};

#define SABLE_INST_PRINTER_TEMPLATE_ARGS                                       \
  template <                                                                   \
      std::output_iterator<char> IteratoryType,                                \
      entity_name_map EntityNameMapType, local_name_map LocalNameMapType>

SABLE_INST_PRINTER_TEMPLATE_ARGS
class InstPrinter :
    public InstVisitorBase<
        InstPrinter<IteratoryType, EntityNameMapType, LocalNameMapType>,
        IteratoryType> {
  IteratoryType Out;
  EntityNameMapType &EntityNameMap;
  LocalNameMapType &LocalNameMap;

public:
  IteratoryType operator()(instructions::Unreachable const *);
  IteratoryType operator()(instructions::Branch const *Inst);
  IteratoryType operator()(instructions::BranchTable const *Inst);
  IteratoryType operator()(instructions::Return const *Inst);
};

#undef SABLE_INST_PRINTER_TEMPLATE_ARGS
} // namespace mir::printer

#endif
