#ifndef SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN
#define SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN

#include "Instruction.h"
#include "Module.h"

#include "../bytecode/Instruction.h"
#include "../bytecode/Module.h"

namespace mir::codegen {
class TranslationModuleContext {
  std::vector<Function *> Functions;
  std::vector<Global *> Globals;
  std::vector<Memory *> Memories;
  std::vector<Table *> Tables;

public:
  Function *operator[](bytecode::FuncIDX const &) const;
  Global *operator[](bytecode::GlobalIDX const &) const;
  Memory *operator[](bytecode::MemIDX const &) const;
  Table *operator[](bytecode::TableIDX const &) const;
};

class TranslationLocalContext {
  struct LabelFrame {
    BasicBlock *PhiBlock;
    BasicBlock *NextBlock;
    unsigned NumPhiInst;
  };
  std::vector<LabelFrame> Labels;
  std::vector<Local> Locals;

public:
  Local *operator[](bytecode::LocalIDX const &) const;
  BasicBlock *getPhiBlock(bytecode::LabelIDX const &) const;
  BasicBlock *getNextBlock(bytecode::LabelIDX const &) const;
  unsigned getNumPhiBlock(bytecode::LabelIDX const &) const;
};

class FunctionTranslator :
    public bytecode::InstVisitorBase<FunctionTranslator, void> {
  TranslationContext &EntityCTX;
  TranslationLocalContext &LocalCTX;
  Function *GFunc;
  bytecode::views::Function BFunc;

  BasicBlock *BB;
  std::vector<Instruction *> OperandStack;

  Instruction *popOperand();
  void pushOperand(Instruction *Operand);

public:
  FunctionTranslator(
      TranslationContext const &EntityCTX_, Function *GFunc_,
      bytecode::views::Function BFunc_);
};

} // namespace mir::codegen

#endif
