#ifndef SABLE_INCLUDE_GUARD_MIR_PASSES_TYPE_INFER
#define SABLE_INCLUDE_GUARD_MIR_PASSES_TYPE_INFER

#include "../Function.h"
#include "../Instruction.h"
#include "Dominator.h"
#include "Pass.h"

#include <memory>
#include <unordered_map>

namespace mir::passes {

class TypeInferPassResult {
public:
  using TypeMap = std::unordered_map<mir::Instruction const *, mir::Type>;

private:
  std::shared_ptr<TypeMap> Types;
  friend class TypeInferPass;
  explicit TypeInferPassResult(std::shared_ptr<TypeMap> Types_);

public:
  TypeInferPassResult() = default;
  mir::Type const &operator[](mir::Instruction const &Instruction) const;
};

class TypeInferPass {
  std::shared_ptr<TypeInferPassResult::TypeMap> Types;
  mir::Function const *Function = nullptr;
  std::shared_ptr<DominatorTreeNode> DomTree;

public:
  void prepare(mir::Function const &Function_);
  void prepare(
      mir::Function const &Function_,
      std::shared_ptr<DominatorTreeNode> DomTree);
  PassStatus run();
  void finalize();

  using AnalysisResult = TypeInferPassResult;
  AnalysisResult getResult() const;

  static constexpr bool isConstantPass() { return true; }
  static constexpr bool isSingleRunPass() { return true; }
  bool isSkipped(mir::Function const &Function_) const;
};

static_assert(function_pass<TypeInferPass>);
} // namespace mir::passes

#endif
