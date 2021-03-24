#ifndef SABLE_INCLUDE_GUARD_MIR_FUNCTION
#define SABLE_INCLUDE_GUARD_MIR_FUNCTION

#include "ASTNode.h"
#include "BasicBlock.h"

#include <range/v3/view/subrange.hpp>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>

namespace mir {
class Module;
class Function;
class Local :
    public ASTNode,
    public llvm::ilist_node_with_parent<Local, Function> {
  friend class Function;
  friend class detail::IListAccessWrapper<Function, Local>;
  friend class detail::IListConstAccessWrapper<Function, Local>;
  Function *Parent;
  bytecode::ValueType Type;
  bool IsParameter;

public:
  explicit Local(bytecode::ValueType Type_);
  bytecode::ValueType const &getType() const;
  bool isParameter() const;
  Function *getParent() const;
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(ASTNode const *Node);
};

class Function :
    public ASTNode,
    public ImportableEntity,
    public ExportableEntity,
    public llvm::ilist_node_with_parent<Function, Module> {
  friend class detail::IListAccessWrapper<Module, Function>;
  friend class detail::IListConstAccessWrapper<Module, Function>;
  Module *Parent;
  bytecode::FunctionType Type;
  llvm::ilist<BasicBlock> BasicBlocks;
  llvm::ilist<Local> Locals;

public:
  explicit Function(bytecode::FunctionType Type_);

  bytecode::FunctionType const &getType() const { return Type; }

  BasicBlock *BuildBasicBlock();
  BasicBlock *BuildBasicBlockAt(BasicBlock *Pos);
  Local *BuildLocal(bytecode::ValueType Type_);
  Local *BuildLocalAt(bytecode::ValueType Type_, Local *Pos);

  bool hasBody() const;
  BasicBlock const &getEntryBasicBlock() const;
  BasicBlock &getEntryBasicBlock();

  detail::IListAccessWrapper<Function, Local> getLocals();
  detail::IListAccessWrapper<Function, BasicBlock> getBasicBlocks();
  detail::IListConstAccessWrapper<Function, Local> getLocals() const;
  detail::IListConstAccessWrapper<Function, BasicBlock> getBasicBlocks() const;

  Module *getParent() const;
  static llvm::ilist<BasicBlock> Function::*getSublistAccess(BasicBlock *);
  static llvm::ilist<Local> Function::*getSublistAccess(Local *);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(ASTNode const *Node);
};
} // namespace mir

#endif