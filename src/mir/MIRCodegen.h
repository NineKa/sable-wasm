#ifndef SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN
#define SABLE_INCLUDE_GUARD_MIR_MIR_CODEGEN

#include "Instruction.h"
#include "Module.h"

#include "../bytecode/Instruction.h"
#include "../bytecode/Module.h"
#include "../parser/customsections/Name.h"

namespace mir::bytecode_codegen {

class ModuleTranslator {
  bytecode::Module const &BModule;
  mir::Module &MIRModule;
  std::vector<Function *> Functions;
  std::vector<Global *> Globals;
  std::vector<Memory *> Memories;
  std::vector<Table *> Tables;

public:
  ModuleTranslator(bytecode::Module const &BModule_, mir::Module &MIRModule_);
  void annotateWith(parser::customsections::Name const &NameSection);

  Function *getMIREntity(bytecode::FuncIDX Index) const;
  Global *getMIREntity(bytecode::GlobalIDX Index) const;
  Memory *getMIREntity(bytecode::MemIDX Index) const;
  Table *getMIREntity(bytecode::TableIDX Index) const;
};

class ExprTranslationContext {

public:
  BasicBlock *getCurrentBasicBlock() const;
  void setCurrentBasicBlock(BasicBlock *BasicBlock_);
  BasicBlock *createBasicBlock() const;
  Instruction *popOperand();
  void pushOperand(Instruction *Operand_);
  bool isReturn(bytecode::LabelIDX Index) const;
  void pushLabel(BasicBlock *PhiBlock, BasicBlock *NextBlock, unsigned NumPhi);
  void popLabel();
  BasicBlock *getPhiBlock(bytecode::LabelIDX Index) const;
  BasicBlock *getNextBlock(bytecode::LabelIDX Index) const;
  unsigned getNumPhi(bytecode::LabelIDX Index) const;
};

} // namespace mir::bytecode_codegen

#endif
