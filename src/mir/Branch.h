#ifndef SABLE_INCLUDE_GUARD_MIR_BRANCH
#define SABLE_INCLUDE_GUARD_MIR_BRANCH

#include "Instruction.h"

namespace mir::instructions {
enum class BranchKind { Unconditional, Conditional, Switch };

namespace branch {
class Unconditional;
class Conditional;
class Switch;
} // namespace branch

class Branch : public Instruction {
  BranchKind Kind;

public:
  explicit Branch(BranchKind Kind_);
  Branch(Branch const &) = delete;
  Branch(Branch &&) noexcept = delete;
  Branch &operator=(Branch const &) = delete;
  Branch &operator=(Branch &&) noexcept = delete;
  ~Branch() noexcept override;

  BranchKind getBranchKind() const;

  bool isConditional() const;
  bool isUnconditional() const;
  bool isSwitch() const;

  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

SABLE_DEFINE_IS_A(Branch)
SABLE_DEFINE_DYN_CAST(Branch)

namespace branch {
class Unconditional : public Branch {
  mir::BasicBlock *Target;

public:
  explicit Unconditional(mir::BasicBlock *Target_);
  Unconditional(Unconditional const &) = delete;
  Unconditional(Unconditional &&) noexcept = delete;
  Unconditional &operator=(Unconditional const &) = delete;
  Unconditional &operator=(Unconditional &&) noexcept = delete;
  ~Unconditional() noexcept override;

  mir::BasicBlock *getTarget() const;
  void setTarget(mir::BasicBlock *Target_);

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Branch const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

class Conditional : public Branch {
  mir::Instruction *Operand;
  mir::BasicBlock *True;
  mir::BasicBlock *False;

public:
  Conditional(
      mir::Instruction *Operand_, mir::BasicBlock *True_,
      mir::BasicBlock *False_);
  Conditional(Conditional const &) = delete;
  Conditional(Conditional &&) noexcept = delete;
  Conditional &operator=(Conditional const &) = delete;
  Conditional &operator=(Conditional &&) noexcept = delete;
  ~Conditional() noexcept override;

  mir::Instruction *getOperand() const;
  void setOperand(mir::Instruction *Operand_);
  mir::BasicBlock *getTrue() const;
  void setTrue(mir::BasicBlock *True_);
  mir::BasicBlock *getFalse() const;
  void setFalse(mir::BasicBlock *False_);

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Branch const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};

class Switch : public Branch {
  mir::Instruction *Operand;
  mir::BasicBlock *DefaultTarget;
  std::vector<mir::BasicBlock *> Targets;

public:
  Switch(
      mir::Instruction *Operand_, mir::BasicBlock *DefaultTarget_,
      std::span<mir::BasicBlock *const> Targets_);
  Switch(Switch const &) = delete;
  Switch(Switch &&) noexcept = delete;
  Switch &operator=(Switch const &) = delete;
  Switch &operator=(Switch &&) noexcept = delete;
  ~Switch() noexcept override;

  mir::Instruction *getOperand() const;
  void setOperand(mir::Instruction *Operand_);

  mir::BasicBlock *getDefaultTarget() const;
  void setDefaultTarget(mir::BasicBlock *DefaultTarget_);

  auto getTargets() {
    auto Begin = ranges::begin(Targets);
    auto End = ranges::end(Targets);
    return ranges::make_subrange(Begin, End);
  }

  auto getTargets() const {
    auto Begin = ranges::begin(Targets);
    auto End = ranges::end(Targets);
    return ranges::make_subrange(Begin, End);
  }

  std::size_t getNumTargets() const;
  mir::BasicBlock *getTarget(std::size_t Index) const;
  void setTarget(std::size_t Index, mir::BasicBlock *Target_);
  void setTargets(std::span<mir::BasicBlock *const> Targets_);

  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(Branch const *Inst);
  static bool classof(Instruction const *Inst);
  static bool classof(ASTNode const *Node);
};
} // namespace branch

template <typename Derived, typename RetType = void, bool Const = true>
class BranchVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <typename T> RetType castAndCall(Ptr<Branch> Inst) {
    return derived()(dyn_cast<T>(Inst));
  }

public:
  RetType visit(Ptr<Branch> Inst) {
    using namespace branch;
    using BKind = BranchKind;
    // clang-format off
    switch (Inst->getBranchKind()) {
    case BKind::Conditional  : return castAndCall<Conditional>(Inst);
    case BKind::Unconditional: return castAndCall<Unconditional>(Inst);
    case BKind::Switch       : return castAndCall<Switch>(Inst);
    default: utility::unreachable();
    }
    // clang-format on
  }
};
} // namespace mir::instructions

#endif