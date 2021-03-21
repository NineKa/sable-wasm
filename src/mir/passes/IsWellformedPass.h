#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_IS_WELLFORMED_PASS
#define SABLE_INCLUDE_GUARD_MIR_PASSES_IS_WELLFORMED_PASS

#include "Pass.h"
#include "ReachingDef.h"

#include <memory>
#include <span>
#include <unordered_set>
#include <vector>

namespace mir::passes {

class IsWellformedPassResult {
public:
  enum class ErrorKind {
    NullOperand,
    InvalidExport,
    InvalidImport,
    InvalidType,
    UnavailableOperand
  };

private:
  using SiteVector = std::vector<std::pair<ASTNode *, ErrorKind>>;
  std::shared_ptr<SiteVector> Sites;

public:
  explicit IsWellformedPassResult(std::shared_ptr<SiteVector> Sites_);

  using iterator = typename SiteVector::iterator;
  iterator begin() const { return Sites->begin(); }
  iterator end() const { return Sites->end(); }

  bool isWellformed() const { return Sites->empty(); }
  std::span<std::pair<ASTNode *, ErrorKind> const> getSites() const {
    return *Sites;
  }
};

class IsWellformedModulePass {
  using ErrorKind = IsWellformedPassResult::ErrorKind;
  using SiteVector = std::vector<std::pair<ASTNode *, ErrorKind>>;
  std::shared_ptr<SiteVector> Sites;
  std::unique_ptr<std::unordered_set<mir::ASTNode const *>> AvailableNodes;
  mir::Module *Module = nullptr;

  struct CheckInitializeExprVisitor;
  void checkInitializeExpr(InitializerExpr *Expr);
  void addSite(ASTNode *Ptr, ErrorKind Reason);

public:
  void prepare(mir::Module &Module_);
  PassStatus run();
  void finalize();
  using AnalysisResult = IsWellformedPassResult;
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
  mir::Function *Function = nullptr;
  std::unique_ptr<ReachingDefPassResult> ReachingDef = nullptr;
  using ErrorKind = IsWellformedPassResult::ErrorKind;
  using SiteVector = std::vector<std::pair<ASTNode *, ErrorKind>>;
  std::shared_ptr<SiteVector> Sites;

  void addSite(ASTNode *Ptr, ErrorKind Reason);

public:
  IsWellformedFunctionPass(IsWellformedModulePass const &ModulePass_);
  void prepare(mir::Function &Function_);
  PassStatus run();
  void finalize();
  bool isSkipped(mir::Function const &) { return false; }
  using AnalysisResult = IsWellformedPassResult;
  AnalysisResult getResult() const;

  static bool isConstantPass() { return true; }
  static bool isSingleRunPass() { return true; }
};

static_assert(module_pass<IsWellformedModulePass>);
static_assert(function_pass<IsWellformedFunctionPass>);
} // namespace mir::passes

#endif
