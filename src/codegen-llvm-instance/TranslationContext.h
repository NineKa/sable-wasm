#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_TRANSLATION_CONTEXT
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_TRANSLATION_CONTEXT

#include "../mir/Function.h"
#include "../mir/passes/Dominator.h"
#include "../mir/passes/TypeInfer.h"

#include <llvm/IR/Function.h>

#include <unordered_map>

namespace codegen::llvm_instance {
class EntityLayout;
class TranslationContext {
  EntityLayout &Layout;
  mir::Function const &Source;
  llvm::Function &Target;

  std::unordered_map<mir::Instruction const *, llvm::Value *> ValueMap;
  std::unordered_map<mir::Local const *, llvm::Value *> LocalMap;
  std::shared_ptr<mir::passes::DominatorTreeNode> DominatorTree;
  mir::passes::TypeInferPassResult TypePassResult;

  std::unordered_map<
      mir::BasicBlock const *,
      std::pair<llvm::BasicBlock *, llvm::BasicBlock *>>
      BasicBlockMap;

  llvm::Value *getLocalInitializer(mir::Local const &Local);

public:
  TranslationContext(
      EntityLayout &Layout_, mir::Function const &Source_,
      llvm::Function &Target_);
  TranslationContext(TranslationContext const &) = delete;
  TranslationContext(TranslationContext &&) noexcept = delete;
  TranslationContext &operator=(TranslationContext const &) = delete;
  TranslationContext &operator=(TranslationContext &&) noexcept = delete;
  ~TranslationContext() noexcept;

  llvm::Value *operator[](mir::Instruction const &Instruction) const;
  llvm::Value *operator[](mir::Local const &Local) const;
  std::pair<llvm::BasicBlock *, llvm::BasicBlock *>
  operator[](mir::BasicBlock const &BasicBlock) const;

  llvm::BasicBlock *createBasicBlock(mir::BasicBlock const &BasicBlock);

  void setValueMapping(mir::Instruction const &Inst, llvm::Value *Value);

  EntityLayout const &getLayout() const;
  std::shared_ptr<mir::passes::DominatorTreeNode> getDominatorTree() const;
  mir::passes::TypeInferPassResult const &getInferredType() const;
  mir::Function const &getSource() const;
  llvm::Function &getTarget();
  llvm::Argument *getInstancePtr() const;
};
} // namespace codegen::llvm_instance

#endif