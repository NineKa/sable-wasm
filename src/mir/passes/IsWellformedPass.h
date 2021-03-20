#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_IS_WELLFORMED_PASS
#define SABLE_INCLUDE_GUARD_MIR_PASSES_IS_WELLFORMED_PASS

#include "Pass.h"

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
  std::span<std::pair<ASTNode *, ErrorKind> const> Sites;

public:
  explicit IsWellformedPassResult(
      std::span<std::pair<ASTNode *, ErrorKind> const> Sites);

  using iterator = decltype(Sites)::iterator;
  iterator begin() const { return Sites.begin(); }
  iterator end() const { return Sites.end(); }

  bool isWellformed() const { return Sites.empty(); }
  std::span<std::pair<ASTNode *, ErrorKind> const> getSites() const {
    return Sites;
  }
};

class IsWellformedModulePass {
  using ErrorKind = IsWellformedPassResult::ErrorKind;
  std::vector<std::pair<ASTNode *, ErrorKind>> Sites;
  std::unordered_set<mir::ASTNode const *> AvailableNodes;

  struct CheckInitializeExprVisitor;
  void checkInitializeExpr(InitializerExpr *Expr);

public:
  PassResult run(mir::Module &Module);
  using AnalysisResult = IsWellformedPassResult;
  AnalysisResult getResult() const;

  bool has(mir::Global const &Global) const;
  bool has(mir::Memory const &Memory) const;
  bool has(mir::Table const &Table) const;
  bool has(mir::Function const &Function) const;
  bool has(mir::DataSegment const &DataSegment) const;
  bool has(mir::ElementSegment const &ElementSegment) const;
};

class IsWellformedFunctionPass {
  IsWellformedModulePass const &ModulePass;

public:
  IsWellformedFunctionPass(IsWellformedModulePass const &ModulePass_);
  PassResult run(mir::Function &Function);
  bool isSkipped(mir::Function const &Function);
  using AnalysisResult = IsWellformedPassResult;
  AnalysisResult getResult() const;
};

static_assert(module_pass<IsWellformedModulePass>);
static_assert(function_pass<IsWellformedFunctionPass>);
} // namespace mir::passes

#endif
