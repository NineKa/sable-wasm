#ifndef SABLE_INCLUDE_GUARD_MIR_BASIC_BLOCK
#define SABLE_INCLUDE_GUARD_MIR_BASIC_BLOCK

#include "ASTNode.h"

#include "Instruction.h"

#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>

#include <vector>

namespace mir {
class Function;

class BasicBlock :
    public ASTNode,
    public llvm::ilist_node_with_parent<BasicBlock, Function> {
  friend class detail::IListAccessWrapper<Function, BasicBlock>;
  friend class detail::IListConstAccessWrapper<Function, BasicBlock>;
  Function *Parent;
  llvm::ilist<Instruction> Instructions;

public:
  BasicBlock();

  template <std::derived_from<Instruction> T, typename... ArgTypes>
  T *BuildInst(ArgTypes &&...Args) {
    auto *Inst = new T(std::forward<ArgTypes>(Args)...);
    push_back(Inst);
    return Inst;
  }

  template <std::derived_from<Instruction> T, typename... ArgTypes>
  T *BuildInstAt(Instruction *Pos, ArgTypes &&...Args) {
    auto *Inst = new T(std::forward<ArgTypes>(Args)...);
    insert(Pos->getIterator(), Inst);
    return Inst;
  }

  using value_type = decltype(Instructions)::value_type;
  using size_type = decltype(Instructions)::size_type;
  using difference_type = decltype(Instructions)::difference_type;
  using reference = decltype(Instructions)::reference;
  using const_reference = decltype(Instructions)::const_reference;
  using pointer = decltype(Instructions)::pointer;
  using const_pointer = decltype(Instructions)::const_pointer;
  using iterator = decltype(Instructions)::iterator;
  using const_iterator = decltype(Instructions)::const_iterator;

  reference front();
  const_reference front() const;
  reference back();
  const_reference back() const;

  iterator insert(iterator Pos, pointer InstPtr);
  iterator insertAfter(iterator Pos, pointer InstPtr);
  void splice(iterator Pos, BasicBlock &Other);
  void splice(iterator Pos, BasicBlock &Other, iterator Begin, iterator End);
  void splice(iterator Pos, pointer InstPtr);

  pointer remove(pointer InstPtr);
  void erase(pointer Inst);
  void push_back(pointer Inst);
  void push_front(pointer Inst);
  void clear();
  void pop_back();
  void pop_front();

  iterator begin();
  const_iterator begin() const;
  const_iterator cbegin() const;

  iterator end();
  const_iterator end() const;
  const_iterator cend() const;

  bool empty() const;
  size_type size() const;

  bool isEntryBlock() const;
  bool hasNoInwardFlow() const;

  llvm::SmallPtrSet<BasicBlock *, 4> getInwardFlow() const;
  llvm::SmallPtrSet<BasicBlock *, 2> getOutwardFlow() const;

  Instruction *getTerminatingInst();

  void eraseFromParent();
  Function *getParent() const;
  static llvm::ilist<Instruction> BasicBlock::*getSublistAccess(Instruction *);
  void replace(ASTNode const *, ASTNode *) noexcept override;
  static bool classof(ASTNode const *Node);
};
} // namespace mir

#endif