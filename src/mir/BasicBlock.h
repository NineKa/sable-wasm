#ifndef SABLE_INCLUDE_GUARD_MIR_BASIC_BLOCK
#define SABLE_INCLUDE_GUARD_MIR_BASIC_BLOCK

#include "ASTNode.h"

#include "Instruction.h"

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>

#include <vector>

namespace mir {
class Function;

class BasicBlock :
    public ASTNode,
    public llvm::ilist_node_with_parent<BasicBlock, Function> {
  Function *Parent;
  llvm::ilist<Instruction> Instructions;

public:
  explicit BasicBlock(Function *Parent_);

  template <std::derived_from<Instruction> T, typename... ArgTypes>
  T *BuildInst(ArgTypes &&...Args) {
    auto *Inst = new T(this, std::forward<ArgTypes>(Args)...);
    Instructions.push_back(Inst);
    return Inst;
  }

  template <std::derived_from<Instruction> T, typename... ArgTypes>
  T *BuildInstAt(Instruction *Before, ArgTypes &&...Args) {
    auto *Inst = new T(this, std::forward<ArgTypes>(Args)...);
    Instructions.insert(Before->getIterator(), Inst);
    return Inst;
  }

  void erase(Instruction *Inst) { Instructions.erase(Inst); }

  Function *getParent() const { return Parent; }

  using iterator = decltype(Instructions)::iterator;
  using const_iterator = decltype(Instructions)::const_iterator;
  iterator begin() { return Instructions.begin(); }
  iterator end() { return Instructions.end(); }
  const_iterator begin() const { return Instructions.begin(); }
  const_iterator end() const { return Instructions.end(); }

  bool empty() const;
  std::vector<BasicBlock *> getInwardFlow() const;
  std::vector<BasicBlock *> getOutwardFlow() const;

  static llvm::ilist<Instruction> BasicBlock::*getSublistAccess(Instruction *);
  void detach(ASTNode const *) noexcept override;
  static bool classof(ASTNode const *Node);
};
} // namespace mir

#endif