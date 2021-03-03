#ifndef SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN
#define SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN

#include "../bytecode/Module.h"
#include "../parser/customsections/Name.h"
#include "Module.h"

#include <vector>

namespace mir::bytecode_codegen {
class EntityMap {
  std::vector<Function *> Functions;
  std::vector<Memory *> Memories;
  std::vector<Global *> Globals;
  std::vector<Table *> Tables;

  template <ranges::random_access_range T, typename IndexType>
  typename T::value_type getPtr(T const &Container, IndexType Index) const;

public:
  EntityMap(bytecode::Module const &BModule, Module &MModule);

  Function *operator[](bytecode::FuncIDX Index) const;
  Memory *operator[](bytecode::MemIDX Index) const;
  Table *operator[](bytecode::TableIDX Index) const;
  Global *operator[](bytecode::GlobalIDX Index) const;

  void annotate(parser::customsections::Name const &Name);
  Memory *getImplicitMemory() const;
  Table *getImplicitTable() const;
};

class TranslationTask {
  BasicBlock *CurrentBB = nullptr;
  BasicBlock *ExitBB = nullptr;
  bytecode::views::Function FunctionView;
  Function &MFunction;

  struct ScopeFrame {
    BasicBlock *MergeBB;
    BasicBlock *NextBB;
    unsigned NumMergePhi;
  };
  std::vector<ScopeFrame> Scopes;
  std::vector<Instruction *> Values;
  std::vector<Local *> Locals; // cached for quick random access
  std::vector<Instruction *> ReturnValueCandidates;

public:
  TranslationTask(
      bytecode::views::Function FunctionView_, Function &MFunction_);

  Local *operator[](bytecode::LocalIDX Index) const;

  BasicBlock *&currentBB() { return CurrentBB; }
  BasicBlock *const &currentBB() const { return CurrentBB; }

  class LabelStackAccessView {
    TranslationTask &CTX;

  public:
    LabelStackAccessView(TranslationTask &CTX_) : CTX(CTX_) {}
    void enter(
        BasicBlock *MergeBasicBlock, BasicBlock *NextBasicBlock,
        unsigned NumMergePhi);
    BasicBlock *getMergeBB(bytecode::LabelIDX Index) const;
    unsigned getNumMergePhi(bytecode::LabelIDX Index) const;
    BasicBlock *exit();
  };

  LabelStackAccessView labels() { return LabelStackAccessView(*this); }

  class ValueStackAccessView {
    TranslationTask &CTX;

  public:
    ValueStackAccessView(TranslationTask &CTX_) : CTX(CTX_) {}
    void push(Instruction *Value);
    Instruction *pop();
  };

  ValueStackAccessView values() { return ValueStackAccessView(*this); }
  bytecode::views::Function function() const { return FunctionView; }
  BasicBlock *createBasicBlock() { return MFunction.BuildBasicBlock(ExitBB); }
  Instruction *collectReturn(unsigned NumReturnValue);

  void perform(EntityMap const &EntityMap_);
};
} // namespace mir::bytecode_codegen

#endif
