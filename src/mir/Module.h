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
  explicit Function(Module *Parent);
  Module *getParent() const { return Parent; }
  bytecode::FunctionType const &getType() const { return Type; }

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

public:
  explicit Local(Function *Parent_, bytecode::ValueType Type_);
  Function *getParent() const { return Parent; }
  bytecode::ValueType const &getType() { return Type; }
};

class Global :
    public llvm::ilist_node_with_parent<Global, Module>,
    public detail::UseSiteTraceable<Global, Instruction> {
  Module *Parent;
  bytecode::GlobalType Type;

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

public:
  explicit Table(Module *Parent_);
  Module *getParent() const { return Parent; }
};
} // namespace mir

#endif
