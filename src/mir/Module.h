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
  std::optional<bytecode::views::Function> BytecodeView;

public:
  Function(Module *Parent_, bytecode::FunctionType Type_);
  Module *getParent() const { return Parent; }
  bytecode::FunctionType const &getType() const { return Type; }

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
  bool isParameter() const { return IsParameter; }
  bytecode::ValueType const &getType() const { return Type; }
};

class Global :
    public llvm::ilist_node_with_parent<Global, Module>,
    public detail::UseSiteTraceable<Global, Instruction> {
  Module *Parent;
  bytecode::GlobalType Type;
  std::optional<bytecode::views::Global> BytecodeView;

public:
  explicit Global(Module *Parent_);
  Module *getParent() const { return Parent; }
  bytecode::GlobalType const &getType() const { return Type; }
};

class Memory :
    public llvm::ilist_node_with_parent<Memory, Module>,
    public detail::UseSiteTraceable<Memory, Instruction> {
  Module *Parent;
  bytecode::MemoryType Type;

public:
  explicit Memory(Module *Parent_);
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
  explicit Table(Module *Parent_);
  Module *getParent() const { return Parent; }
};

class Module {
  llvm::ilist<Function> Functions;
  llvm::ilist<Global> Globals;
  llvm::ilist<Memory> Memories;
  llvm::ilist<Table> Tables;

public:
  static llvm::ilist<Function> Module::*getSublistAccess(Function *);
  static llvm::ilist<Global> Module::*getSublistAccess(Global *);
  static llvm::ilist<Memory> Module::*getSublistAccess(Memory *);
  static llvm::ilist<Table> Module::*getSublistAccess(Table *);
};
} // namespace mir

#endif
