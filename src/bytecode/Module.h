#ifndef SABLE_INCLUDE_GUARD_BYTECODE_MODULE
#define SABLE_INCLUDE_GUARD_BYTECODE_MODULE

#include "Instruction.h"
#include "Type.h"

#include <range/v3/core.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/const.hpp>
#include <range/v3/view/transform.hpp>

#include <memory>
#include <variant>
#include <vector>

namespace bytecode {

// clang-format off
using ImportDescriptor =
  std::variant<TypeIDX, TableType, MemoryType, GlobalType>;
using ExportDescriptor = std::variant<FuncIDX, TableIDX, MemIDX, GlobalIDX>;
// clang-format on

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
  std::string_view getExportName() const { return Export->Name; }
  bool isDeclaration() const { return this->isImported(); }
  bool isDefinition() const { return !this->isImported(); }
};

class Table : public Entity<TableType> {
  friend class bytecode::ModuleView;
  TableIDX Index;

public:
  TableIDX getIndex() const { return Index; }
};

class Memory : public Entity<MemoryType> {
  friend class bytecode::ModuleView;
  MemIDX Index;

public:
  MemIDX getIndex() const { return Index; }
};

class Function : public Entity<FunctionType> {
  friend class bytecode::ModuleView;
  entities::Function const *Entity = nullptr;
  FuncIDX Index;

public:
  FuncIDX getIndex() const { return Index; }
  ValueType operator[](LocalIDX const &Index_) const {
    auto Parameters = getType()->getParamTypes();
    auto CastedIndex = static_cast<std::size_t>(Index_);
    if (CastedIndex < ranges::size(Parameters)) return Parameters[CastedIndex];
    CastedIndex = CastedIndex - ranges::size(Parameters);
    if (CastedIndex < ranges::size(Entity->Locals))
      return Entity->Locals[CastedIndex];
    SABLE_UNREACHABLE();
  }
  std::optional<ValueType> get(LocalIDX const &Index_) const {
    auto Parameters = getType()->getParamTypes();
    auto CastedIndex = static_cast<std::size_t>(Index_);
    if (CastedIndex < ranges::size(Parameters)) return Parameters[CastedIndex];
    CastedIndex = CastedIndex - ranges::size(Parameters);
    if (CastedIndex < ranges::size(Entity->Locals))
      return Entity->Locals[CastedIndex];
    return std::nullopt;
  }
  auto getLocalsWithoutArgs() const {
    return ranges::views::all(Entity->Locals);
  }
  auto getLocals() const {
    return ranges::views::concat(
        getType()->getParamTypes(), getLocalsWithoutArgs());
  }
  bytecode::Expression const *getBody() const {
    return std::addressof(Entity->Body);
  }
  bool isDefinition() const { return Entity != nullptr; }
};

class Global : public Entity<GlobalType> {
  friend class bytecode::ModuleView;
  entities::Global const *Entity = nullptr;
  GlobalIDX Index;

public:
  GlobalIDX getIndex() const { return Index; }
  bytecode::Expression const *getInitializer() const {
    return std::addressof(Entity->Initializer);
  }
};

} // namespace views

// a copy-efficient read only view on module storage
// the construct of the view is not-constant, linear regards to the Import
class ModuleView {
  struct ViewStorage {
    Module const *M;
    size_t NumImportedTables;
    size_t NumImportedMemories;
    size_t NumImportedGlobals;
    size_t NumImportedFunctions;
    // cache the indices, as we do not want to iterate through import vector for
    // each indices query
    std::vector<views::Table> Tables;
    std::vector<views::Memory> Memories;
    std::vector<views::Global> Globals;
    std::vector<views::Function> Functions;
  };
  std::shared_ptr<ViewStorage> Storage;

public:
  explicit ModuleView(Module const &M_);
  Module const &module() const { return *(Storage->M); }

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

  size_t getNumImportedTables() const { return Storage->NumImportedTables; }
  size_t getNumImportedMemories() const { return Storage->NumImportedMemories; }
  size_t getNumImportedGlobals() const { return Storage->NumImportedGlobals; }
  size_t getNumImportedFunctions() const {
    return Storage->NumImportedFunctions;
  }
};

} // namespace bytecode

#endif
