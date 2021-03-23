#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_IS_WELLFORMED_PASS
#define SABLE_INCLUDE_GUARD_MIR_PASSES_IS_WELLFORMED_PASS

#include "Dominator.h"
#include "Pass.h"

#include <memory>
#include <span>
#include <vector>

namespace mir::passes {

class IsWellformedCallback {
public:
  IsWellformedCallback() = default;
  IsWellformedCallback(IsWellformedCallback const &) = default;
  IsWellformedCallback(IsWellformedCallback &&) noexcept = default;
  IsWellformedCallback &operator=(IsWellformedCallback const &) = default;
  IsWellformedCallback &operator=(IsWellformedCallback &&) noexcept = default;
  virtual ~IsWellformedCallback() noexcept = default;

  virtual void hasNullOperand(ASTNode const *Node) = 0;
  virtual void hasInvalidType(ASTNode const *Node) = 0;
  virtual void hasInvalidImport(ImportableEntity const *Node) = 0;
  virtual void hasInvalidExport(ExportableEntity const *Node) = 0;
  virtual void referUnavailable(ASTNode const *Node) = 0;
  virtual void
  referNonDominating(Instruction const *Node, Instruction const *Operand) = 0;
  virtual void hasInvalidOperator(Instruction const *Node) = 0;
  virtual void referNonDominatingPhi(
      Instruction const *Node, Instruction const *Value,
      BasicBlock const *Path) = 0;
  virtual void hasPhiAfterMerge(instructions::Phi const *PhiNode) = 0;
  virtual void appearAfterTerminatingInst(Instruction const *Node) = 0;
  virtual void missingTerminatingInst(BasicBlock const *Node) = 0;
};

class IsWellformedCallbackTrivial : public IsWellformedCallback {
  bool IsWellformed = true;

public:
  void hasNullOperand(ASTNode const *Node) override;
  void hasInvalidType(ASTNode const *Node) override;
  void hasInvalidImport(ImportableEntity const *Node) override;
  void hasInvalidExport(ExportableEntity const *Node) override;
  void referUnavailable(ASTNode const *Node) override;
  void referNonDominating(
      Instruction const *Node, Instruction const *Operand) override;
  void hasInvalidOperator(Instruction const *Node) override;
  void referNonDominatingPhi(
      Instruction const *Node, Instruction const *Value,
      BasicBlock const *Path) override;
  void hasPhiAfterMerge(instructions::Phi const *PhiNode) override;
  void appearAfterTerminatingInst(Instruction const *Node) override;
  void missingTerminatingInst(BasicBlock const *Node) override;
  bool isWellformed() const { return IsWellformed; }
  operator bool() const { return IsWellformed; }
};

class IsWellformedFunctionPass;
class IsWellformedModulePass {
  friend class IsWellformedFunctionPass;
  std::shared_ptr<IsWellformedCallback> Callback;
  std::unique_ptr<std::vector<mir::ASTNode const *>> AvailableNodes;
  mir::Module const *Module = nullptr;

  struct CheckInitializeExprVisitor;
  void checkInitializeExpr(InitializerExpr *Expr);

public:
  explicit IsWellformedModulePass(
      std::shared_ptr<IsWellformedCallback> Callback_ =
          std::make_shared<IsWellformedCallbackTrivial>());

  void prepare(mir::Module const &Module_);
  PassStatus run();
  void finalize();
  using AnalysisResult = IsWellformedCallback const &;
  AnalysisResult getResult() const;

  bool has(mir::Global const &Global) const;
  bool has(mir::Memory const &Memory) const;
  bool has(mir::Table const &Table) const;
  bool has(mir::Function const &Function) const;
  bool has(mir::Data const &DataSegment) const;
  bool has(mir::Element const &ElementSegment) const;

  static constexpr bool isConstantPass() { return true; }
  static constexpr bool isSingleRunPass() { return true; }
};

class IsWellformedFunctionPass {
  IsWellformedModulePass const *ModulePass;
  std::shared_ptr<IsWellformedCallback> Callback;

  mir::Function const *Function = nullptr;
  std::unique_ptr<DominatorPassResult> Dominator;
  std::unique_ptr<std::vector<mir::BasicBlock const *>> AvailableBB;
  std::unique_ptr<std::vector<mir::Local const *>> AvailableLocal;

  struct CheckInstVisitor;

public:
  explicit IsWellformedFunctionPass(IsWellformedModulePass const &ModulePass_);
  void prepare(mir::Function const &Function_);
  PassStatus run();
  void finalize();
  bool isSkipped(mir::Function const &) { return false; }
  using AnalysisResult = IsWellformedCallback const &;
  AnalysisResult getResult() const;

  bool has(mir::BasicBlock const &BasicBlock) const;
  bool has(mir::Local const &Local) const;

  static bool constexpr isConstantPass() { return true; }
  static bool constexpr isSingleRunPass() { return true; }
};

static_assert(module_pass<IsWellformedModulePass>);
static_assert(function_pass<IsWellformedFunctionPass>);
} // namespace mir::passes

namespace mir {
bool validate(mir::Module const &Module);
} // namespace mir

#endif
