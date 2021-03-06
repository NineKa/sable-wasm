#ifndef SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN
#define SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN

#include "../bytecode/Module.h"
#include "../parser/customsections/Name.h"
#include "Module.h"

#include <vector>

namespace mir::bytecode_codegen {
class EntityMap {
  std::vector<Function *> Functions;
  std::vector<Memory *> Memories;
  std::vector<Global *> Globals;
  std::vector<Table *> Tables;

  template <ranges::random_access_range T, typename IndexType>
  typename T::value_type getPtr(T const &Container, IndexType Index) const;

public:
  EntityMap(bytecode::Module const &BModule, Module &MModule);

  Function *operator[](bytecode::FuncIDX Index) const;
  Memory *operator[](bytecode::MemIDX Index) const;
  Table *operator[](bytecode::TableIDX Index) const;
  Global *operator[](bytecode::GlobalIDX Index) const;

  void annotate(parser::customsections::Name const &Name);
  Memory *getImplicitMemory() const;
  Table *getImplicitTable() const;
};

} // namespace mir::bytecode_codegen

#endif
