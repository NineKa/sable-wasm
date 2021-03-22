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
  virtual void hasNullOperand(ASTNode *Node) = 0;
  virtual void hasInvalidType(ASTNode *Node) = 0;
  virtual void hasInvalidImport(ImportableEntity *Node) = 0;
  virtual void hasInvalidExport(ExportableEntity *Node) = 0;
  virtual void referUnavailable(ASTNode *Node) = 0;
  virtual void referNonDominating(Instruction *Node, Instruction *Operand) = 0;
  virtual void hasInvalidOperator(Instruction *Node) = 0;
  virtual void referNonDominatingPhi(
      Instruction *Node, Instruction *Value, BasicBlock *Path) = 0;
  virtual void hasPhiAfterMerge(instructions::Phi *PhiNode) = 0;
  virtual void appearAfterTerminatingInst(Instruction *Node) = 0;
  virtual void missingTerminatingInst(BasicBlock *Node) = 0;
};

class IsWellformedCallbackTrivial : public IsWellformedCallback {
  bool IsWellformed = true;

public:
  void hasNullOperand(ASTNode *Node) override;
  void hasInvalidType(ASTNode *Node) override;
  void hasInvalidImport(ImportableEntity *Node) override;
  void hasInvalidExport(ExportableEntity *Node) override;
  void referUnavailable(ASTNode *Node) override;
  void referNonDominating(Instruction *Node, Instruction *Operand) override;
  void hasInvalidOperator(Instruction *Node) override;
  void referNonDominatingPhi(
      Instruction *Node, Instruction *Value, BasicBlock *Path) override;
  void hasPhiAfterMerge(instructions::Phi *PhiNode) override;
  void appearAfterTerminatingInst(Instruction *Node) override;
  void missingTerminatingInst(BasicBlock *Node) override;
  bool isWellformed() const { return IsWellformed; }
  operator bool() const { return IsWellformed; }
};

class IsWellformedFunctionPass;
class IsWellformedModulePass {
  friend class IsWellformedFunctionPass;
  std::shared_ptr<IsWellformedCallback> Callback;
  std::unique_ptr<std::vector<mir::ASTNode const *>> AvailableNodes;
  mir::Module *Module = nullptr;

  struct CheckInitializeExprVisitor;
  void checkInitializeExpr(InitializerExpr *Expr);

public:
  explicit IsWellformedModulePass(
      std::shared_ptr<IsWellformedCallback> Callback_ =
          std::make_shared<IsWellformedCallbackTrivial>());

  void prepare(mir::Module &Module_);
  PassStatus run();
  void finalize();
  using AnalysisResult = IsWellformedCallback const &;
  AnalysisResult getResult() const;

  bool has(mir::Global const &Global) const;
  bool has(mir::Memory const &Memory) const;
  bool has(mir::Table const &Table) const;
  bool has(mir::Function const &Function) const;
  bool has(mir::DataSegment const &DataSegment) const;
  bool has(mir::ElementSegment const &ElementSegment) const;

  static bool isConstantPass() { return true; }
  static bool isSingleRunPass() { return true; }
};

class IsWellformedFunctionPass {
  IsWellformedModulePass const *ModulePass;
  std::shared_ptr<IsWellformedCallback> Callback;

  mir::Function *Function = nullptr;
  std::unique_ptr<DominatorPassResult> Dominator;
  std::unique_ptr<std::vector<mir::BasicBlock const *>> AvailableBB;
  std::unique_ptr<std::vector<mir::Local const *>> AvailableLocal;

  struct CheckInstVisitor;

public:
  explicit IsWellformedFunctionPass(IsWellformedModulePass const &ModulePass_);
  void prepare(mir::Function &Function_);
  PassStatus run();
  void finalize();
  bool isSkipped(mir::Function const &) { return false; }
  using AnalysisResult = IsWellformedCallback const &;
  AnalysisResult getResult() const;

  bool has(mir::BasicBlock const &BasicBlock) const;
  bool has(mir::Local const &Local) const;

  static bool isConstantPass() { return true; }
  static bool isSingleRunPass() { return true; }
};

static_assert(module_pass<IsWellformedModulePass>);
static_assert(function_pass<IsWellformedFunctionPass>);
} // namespace mir::passes

#endif
