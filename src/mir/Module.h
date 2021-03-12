#ifndef SABLE_INCLUDE_GUARD_MIR_MODULE
#define SABLE_INCLUDE_GUARD_MIR_MODULE

#include "../bytecode/Module.h"
#include "ASTNode.h"
#include "Instruction.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>

namespace mir {

namespace detail {
using ImportDescriptor = std::pair<std::string, std::string>;
using ExportDescriptor = std::string;
class ImportableEntity {
  std::unique_ptr<ImportDescriptor> Import = nullptr;

public:
  bool isImported() const;
  std::string_view getImportModuleName() const;
  std::string_view getImportEntityName() const;
  void setImport(std::string ModuleName, std::string EntityName);
};

class ExportableEntity {
  std::unique_ptr<ExportDescriptor> Export = nullptr;

public:
  bool isExported() const;
  std::string_view getExportName() const;
  void setExport(std::string EntityName);
};
} // namespace detail

class Function :
    public ASTNode,
    public detail::ImportableEntity,
    public detail::ExportableEntity,
    public llvm::ilist_node_with_parent<Function, Module> {
  Module *Parent;
  bytecode::FunctionType Type;
  llvm::ilist<BasicBlock> BasicBlocks;
  llvm::ilist<Local> Locals;

public:
  Function(Module *Parent_, bytecode::FunctionType Type_);

  Module *getParent() const { return Parent; }
  bytecode::FunctionType const &getType() const { return Type; }

  BasicBlock const *getEntryBasicBlock() const;
  BasicBlock *getEntryBasicBlock();

  using bb_iterator = decltype(BasicBlocks)::iterator;
  using bb_const_iterator = decltype(BasicBlocks)::const_iterator;
  bb_iterator basic_block_begin() { return BasicBlocks.begin(); }
  bb_iterator basic_block_end() { return BasicBlocks.end(); }
  bb_const_iterator basic_block_begin() const { return BasicBlocks.begin(); }
  bb_const_iterator basic_block_end() const { return BasicBlocks.end(); }

  auto getBasicBlocks() {
    return ranges::subrange(basic_block_begin(), basic_block_end());
  }
  auto getBasicBlocks() const {
    return ranges::subrange(basic_block_begin(), basic_block_end());
  }

  using local_iterator = decltype(Locals)::iterator;
  using local_const_iterator = decltype(Locals)::const_iterator;
  local_iterator local_begin() { return Locals.begin(); }
  local_iterator local_end() { return Locals.end(); }
  local_const_iterator local_begin() const { return Locals.begin(); }
  local_const_iterator local_end() const { return Locals.end(); }

  auto getLocals() { return ranges::subrange(local_begin(), local_end()); }
  auto getLocals() const {
    return ranges::subrange(local_begin(), local_end());
  }

  BasicBlock *BuildBasicBlock(BasicBlock *Before = nullptr);
  Local *BuildLocal(bytecode::ValueType Type_, Local *Before = nullptr);

  void erase(BasicBlock *BasicBlock_);
  void erase(Local *Local_);

  static llvm::ilist<BasicBlock> Function::*getSublistAccess(BasicBlock *) {
    return &Function::BasicBlocks;
  }
  static llvm::ilist<Local> Function::*getSublistAccess(Local *) {
    return &Function::Locals;
  }

  void detach(ASTNode const *) noexcept override { SABLE_UNREACHABLE(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Function;
  }
};

class Local :
    public ASTNode,
    public llvm::ilist_node_with_parent<Local, Function> {
  Function *Parent;
  bytecode::ValueType Type;
  bool IsParameter;

  friend class Function;
  struct IsParameterTag {};
  Local(IsParameterTag, Function *Parent_, bytecode::ValueType Type_);

public:
  Local(Function *Parent_, bytecode::ValueType Type_);
  Function *getParent() const { return Parent; }
  bytecode::ValueType const &getType() const { return Type; }
  bool isParameter() const { return IsParameter; }
  void detach(ASTNode const *) noexcept override { SABLE_UNREACHABLE(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Local;
  }
};

class Global :
    public ASTNode,
    public detail::ImportableEntity,
    public detail::ExportableEntity,
    public llvm::ilist_node_with_parent<Global, Module> {
  Module *Parent;
  bytecode::GlobalType Type;

public:
  Global(Module *Parent_, bytecode::GlobalType Type_);
  Module *getParent() const { return Parent; }
  bytecode::GlobalType const &getType() const { return Type; }
  void detach(ASTNode const *) noexcept override { SABLE_UNREACHABLE(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Global;
  }
};

class Memory :
    public ASTNode,
    public detail::ImportableEntity,
    public detail::ExportableEntity,
    public llvm::ilist_node_with_parent<Memory, Module> {
  Module *Parent;
  bytecode::MemoryType Type;

public:
  Memory(Module *Parent_, bytecode::MemoryType Type_);
  Module *getParent() const { return Parent; }
  bytecode::MemoryType const &getType() const { return Type; }
  void detach(ASTNode const *) noexcept override { SABLE_UNREACHABLE(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Memory;
  }
};

class Table :
    public ASTNode,
    public detail::ImportableEntity,
    public detail::ExportableEntity,
    public llvm::ilist_node_with_parent<Table, Module> {
  Module *Parent;
  bytecode::TableType Type;

public:
  Table(Module *Parent_, bytecode::TableType Type_);
  Module *getParent() const { return Parent; }
  bytecode::TableType const &getType() const { return Type; }
  void detach(ASTNode const *) noexcept override { SABLE_UNREACHABLE(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Table;
  }
};

class Module : public ASTNode {
  llvm::ilist<Function> Functions;
  llvm::ilist<Global> Globals;
  llvm::ilist<Memory> Memories;
  llvm::ilist<Table> Tables;

public:
  Module();
  using func_iterator = decltype(Functions)::iterator;
  using func_const_iterator = decltype(Functions)::const_iterator;
  func_iterator function_begin() { return Functions.begin(); }
  func_iterator function_end() { return Functions.end(); }
  func_const_iterator function_begin() const { return Functions.begin(); }
  func_const_iterator function_end() const { return Functions.end(); }

  auto getFunctions() {
    return ranges::subrange(function_begin(), function_end());
  }
  auto getFunctions() const {
    return ranges::subrange(function_begin(), function_end());
  }

  using global_iterator = decltype(Globals)::iterator;
  using global_const_iterator = decltype(Globals)::const_iterator;
  global_iterator global_begin() { return Globals.begin(); }
  global_iterator global_end() { return Globals.end(); }
  global_const_iterator global_begin() const { return Globals.begin(); }
  global_const_iterator global_end() const { return Globals.end(); }

  auto getGlobals() { return ranges::subrange(global_begin(), global_end()); }
  auto getGlobals() const {
    return ranges::subrange(global_begin(), global_end());
  }

  using mem_iterator = decltype(Memories)::iterator;
  using mem_const_iterator = decltype(Memories)::const_iterator;
  mem_iterator memory_begin() { return Memories.begin(); }
  mem_iterator memory_end() { return Memories.end(); }
  mem_const_iterator memory_begin() const { return Memories.begin(); }
  mem_const_iterator memory_end() const { return Memories.end(); }

  auto getMemories() { return ranges::subrange(memory_begin(), memory_end()); }
  auto getMemories() const {
    return ranges::subrange(memory_begin(), memory_end());
  }

  using table_iterator = decltype(Tables)::iterator;
  using table_const_iterator = decltype(Tables)::const_iterator;
  table_iterator table_begin() { return Tables.begin(); }
  table_iterator table_end() { return Tables.end(); }
  table_const_iterator table_begin() const { return Tables.begin(); }
  table_const_iterator table_end() const { return Tables.end(); }

  auto getTables() { return ranges::subrange(table_begin(), table_end()); }
  auto getTables() const {
    return ranges::subrange(table_begin(), table_end());
  }

  Function *BuildFunction(bytecode::FunctionType Type_);
  Global *BuildGlobal(bytecode::GlobalType Type_);
  Memory *BuildMemory(bytecode::MemoryType Type_);
  Table *BuildTable(bytecode::TableType Type_);

  static llvm::ilist<Function> Module::*getSublistAccess(Function *) {
    return &Module::Functions;
  }
  static llvm::ilist<Global> Module::*getSublistAccess(Global *) {
    return &Module::Globals;
  }
  static llvm::ilist<Memory> Module::*getSublistAccess(Memory *) {
    return &Module::Memories;
  }
  static llvm::ilist<Table> Module::*getSublistAccess(Table *) {
    return &Module::Tables;
  }

  void detach(ASTNode const *) noexcept override { SABLE_UNREACHABLE(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Module;
  }
};
} // namespace mir

#endif
