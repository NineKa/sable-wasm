#ifndef SABLE_INCLUDE_GUARD_MIR_MODULE
#define SABLE_INCLUDE_GUARD_MIR_MODULE

#include "../bytecode/Module.h"
#include "Instruction.h"

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>

namespace mir {

class Function :
    public llvm::ilist_node_with_parent<Function, Module>,
    public detail::UseSiteTraceable<Function, Instruction> {
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

  bool isVoidReturn() const;
  bool isMultiValueReturn() const;
  bool isSingleValueReturn() const;

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
};

class Local :
    public llvm::ilist_node_with_parent<Local, Function>,
    public detail::UseSiteTraceable<Local, Instruction> {
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
};

class Global :
    public llvm::ilist_node_with_parent<Global, Module>,
    public detail::UseSiteTraceable<Global, Instruction> {
  Module *Parent;
  bytecode::GlobalType Type;

public:
  Global(Module *Parent_, bytecode::GlobalType Type_);
  Module *getParent() const { return Parent; }
  bytecode::GlobalType const &getType() const { return Type; }
};

class Memory :
    public llvm::ilist_node_with_parent<Memory, Module>,
    public detail::UseSiteTraceable<Memory, Instruction> {
  Module *Parent;
  bytecode::MemoryType Type;

public:
  Memory(Module *Parent_, bytecode::MemoryType Type_);
  Module *getParent() const { return Parent; }
  bytecode::MemoryType const &getType() const { return Type; }
};

class Table :
    public llvm::ilist_node_with_parent<Table, Module>,
    public detail::UseSiteTraceable<Table, Instruction> {
  Module *Parent;
  bytecode::TableType Type;
  std::optional<bytecode::views::Table> BytecodeView;

public:
  Table(Module *Parent_, bytecode::TableType Type_);
  Module *getParent() const { return Parent; }
  bytecode::TableType const &getType() const { return Type; }
};

class Module {
  llvm::ilist<Function> Functions;
  llvm::ilist<Global> Globals;
  llvm::ilist<Memory> Memories;
  llvm::ilist<Table> Tables;

public:
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
    return ranges::subrange(function_end(), function_end());
  }

  using global_iterator = decltype(Globals)::iterator;
  using global_const_iterator = decltype(Globals)::const_iterator;
  global_iterator global_begin() { return Globals.begin(); }
  global_iterator global_end() { return Globals.end(); }
  global_const_iterator global_begin() const { return Globals.begin(); }
  global_const_iterator global_end() const { return Globals.end(); }

  auto getGlobal() { return ranges::subrange(global_begin(), global_end()); }
  auto getGlobal() const {
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
};
} // namespace mir

#endif
