#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_IS_WELLFORMED_PASS
#define SABLE_INCLUDE_GUARD_MIR_PASSES_IS_WELLFORMED_PASS

#include "Pass.h"

#include <span>
#include <unordered_set>
#include <vector>

namespace mir::passes {

class IsWellformedPassAnalaysisResult {
public:
  enum class ErrorKind {
    NullOpearnd,
    InvalidExport,
    InvalidImport,
    UnavailableASTNode
  };

private:
  std::span<std::pair<ASTNode *, ErrorKind> const> Sites;

public:
  IsWellformedPassAnalaysisResult(std::span<ASTNode *const> Sites);

  using iterator = decltype(Sites)::iterator;
  iterator begin() const { return Sites.begin(); }
  iterator end() const { return Sites.end(); }

  bool isWellformed() const { return Sites.empty(); }
  std::span<std::pair<ASTNode *, ErrorKind> const> getSites() const {
    return Sites;
  }
};

class IsWellformedModulePass {
  using ErrorKind = IsWellformedPassAnalaysisResult::ErrorKind;
  std::vector<std::pair<ASTNode *, ErrorKind>> Sites;
  std::unordered_set<mir::ASTNode const *> AvailableNodes;

public:
  void run(mir::Module &Module);
  using AnalysisResult = IsWellformedPassAnalaysisResult;
  AnalysisResult getResult() const;

  bool has(mir::Global const &Global) const;
  bool has(mir::Memory const &Memory) const;
  bool has(mir::Table const &Table) const;
  bool has(mir::Function const &Function) const;
  bool has(mir::DataSegment const &DataSegment) const;
  bool has(mir::ElementSegment const &ElementSegment) const;
};

class IsWellformedFuntionPass {
  IsWellformedModulePass const &ModulePass;

public:
  IsWellformedFuntionPass(IsWellformedModulePass const &ModulePass_);
  void run(mir::Function &Function);
  bool isSkipped(mir::Function const &Function);
  using AnalysisResult = IsWellformedPassAnalaysisResult;
  AnalysisResult getResult() const;
};

static_assert(module_pass<IsWellformedModulePass>);
static_assert(function_pass<IsWellformedFuntionPass>);
} // namespace mir::passes

#endif
