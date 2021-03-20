#include "LLVMCodege.h"

#include <unordered_map>

namespace codegen::llvm_instance {

class FunctionTranslationTask::TranslationContext {
  std::unordered_map<mir::Instruction const *, llvm::Value *> ValueMap;
  std::unordered_map<mir::BasicBlock const *, llvm::BasicBlock *> BasicBlockMap;

public:
  llvm::Value *operator[](mir::Instruction const *) const;
};

class FunctionTranslationTask::TranslationVisitor :
    mir::InstVisitorBase<TranslationVisitor, llvm::Value *> {};

} // namespace codegen::llvm_instance