#ifndef SABLE_INCLUDE_GUARD_BYTECODE_MODULE
#define SABLE_INCLUDE_GUARD_BYTECODE_MODULE

#include "Instruction.h"
#include "Type.h"

#include <range/v3/core.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/const.hpp>
#include <range/v3/view/transform.hpp>

#include <memory>
#include <variant>
#include <vector>

namespace bytecode {

using ImportDescriptor =
    std::variant<TypeIDX, TableType, MemoryType, GlobalType>;
using ExportDescriptor = std::variant<FuncIDX, TableIDX, MemIDX, GlobalIDX>;

namespace entities {
struct Function {
  bytecode::TypeIDX Type;
  std::vector<bytecode::ValueType> Locals;
  bytecode::Expression Body;
};

struct Table {
  bytecode::TableType Type;
};

struct Memory {
  bytecode::MemoryType Type;
};

struct Global {
  bytecode::GlobalType Type;
  bytecode::Expression Initializer;
};

struct Element {
  bytecode::TableIDX Table;
  bytecode::Expression Offset;
  std::vector<bytecode::FuncIDX> Initializer;
};

struct Data {
  bytecode::MemIDX Memory;
  bytecode::Expression Offset;
  std::vector<std::byte> Initializer;
};

struct Export {
  std::string Name;
  ExportDescriptor Descriptor;
};

struct Import {
  std::string ModuleName;
  std::string EntityName;
  ImportDescriptor Descriptor;
};
} // namespace entities

struct Module {
  // clang-format off
  std::vector  <bytecode::FunctionType> Types;
  std::vector  <entities::Function    > Functions;
  std::vector  <entities::Table       > Tables;
  std::vector  <entities::Memory      > Memories;
  std::vector  <entities::Global      > Globals;
  std::vector  <entities::Element     > Elements;
  std::vector  <entities::Data        > Data;
  std::optional<bytecode::FuncIDX     > Start;
  std::vector  <entities::Import      > Imports;
  std::vector  <entities::Export      > Exports;
  // clang-format on
};

class ModuleView;

namespace views {
template <typename EntityType> class Entity {
  friend class bytecode::ModuleView;
  entities::Import const *Import = nullptr;
  entities::Export const *Export = nullptr;
  EntityType const *Type = nullptr;

public:
  EntityType const *getType() const { return Type; }
  bool isImported() const { return Import != nullptr; }
  std::string_view getImportModuleName() const { return Import->ModuleName; }
  std::string_view getImportEntityName() const { return Import->EntityName; }
  bool isExported() const { return Export != nullptr; }
  std::string_view getExportName() { return Export->Name; }
};

// clang-format off
class Table : public Entity<TableType> { friend class bytecode::ModuleView; };
class Memory : public Entity<MemoryType> { friend class bytecode::ModuleView; };

class Function : public Entity<FunctionType> {
  friend class bytecode::ModuleView;
  entities::Function const *Entity = nullptr;
public:
  ValueType operator[](LocalIDX const & Index) const {
    auto Parameters = getType()->getParamTypes(); 
    auto CastedIndex = static_cast<std::size_t>(Index);
    if (CastedIndex < ranges::size(Parameters)) return Parameters[CastedIndex];
    CastedIndex = CastedIndex - ranges::size(Parameters);
    if (CastedIndex < ranges::size(Entity->Locals)) 
      return Entity->Locals[CastedIndex];
    SABLE_UNREACHABLE();
  }
  std::optional<ValueType> get(LocalIDX const &Index) const {
    auto Parameters = getType()->getParamTypes(); 
    auto CastedIndex = static_cast<std::size_t>(Index);
    if (CastedIndex < ranges::size(Parameters)) return Parameters[CastedIndex];
    CastedIndex = CastedIndex - ranges::size(Parameters);
    if (CastedIndex < ranges::size(Entity->Locals)) 
      return Entity->Locals[CastedIndex];
    return std::nullopt;
  }
  bytecode::Expression const *getBody() const
  { return std::addressof(Entity->Body); }
};

class Global : public Entity<GlobalType> {
  friend class bytecode::ModuleView;
  entities::Global const *Entity = nullptr;
public:
  bytecode::Expression const *getInitializer() const
  { return std::addressof(Entity->Initializer); }
};
// clang-format on
} // namespace views

// a copy-efficient read only view on module storage
// the construct of the view is not-constant, linear regards to the Import
class ModuleView {
  struct ViewStorage {
    Module const *M;
    // cache the indices, as we do not want to iterate through import vector for
    // each indices query
    std::vector<views::Table> Tables;
    std::vector<views::Memory> Memories;
    std::vector<views::Global> Globals;
    std::vector<views::Function> Functions;
  };
  std::shared_ptr<ViewStorage> Storage;

public:
  ModuleView(Module const &M_);

  auto types() const {
    auto TransformFn = [](FunctionType const &Type) {
      return std::addressof(Type);
    };
    return Storage->M->Types | ranges::views::transform(TransformFn);
  }
  auto tables() const { return ranges::views::const_(Storage->Tables); }
  auto memories() const { return ranges::views::const_(Storage->Memories); }
  auto globals() const { return ranges::views::const_(Storage->Globals); }
  auto functions() const { return ranges::views::const_(Storage->Functions); }

  FunctionType const *operator[](TypeIDX const &Index) const;
  views::Table const &operator[](TableIDX const &Index) const;
  views::Memory const &operator[](MemIDX const &Index) const;
  views::Global const &operator[](GlobalIDX const &Index) const;
  views::Function const &operator[](FuncIDX const &Index) const;

  std::optional<FunctionType const *> get(TypeIDX const &Index) const;
  std::optional<views::Table> get(TableIDX const &Index) const;
  std::optional<views::Memory> get(MemIDX const &Index) const;
  std::optional<views::Global> get(GlobalIDX const &Index) const;
  std::optional<views::Function> get(FuncIDX const &Index) const;
};

} // namespace bytecode

#endif
