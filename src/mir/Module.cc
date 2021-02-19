#include "Module.h"

namespace mir {
Function::Function(Module *Parent_, bytecode::FunctionType Type_)
    : Parent(Parent_), Type(std::move(Type_)) {
  for (auto const &ValueType : Type.getParamTypes()) {
    auto *AllocatedParameter =
        new Local(Local::IsParameterTag{}, this, ValueType);
    Locals.push_back(AllocatedParameter);
  }
}

BasicBlock *Function::BuildBasicBlock(BasicBlock *Before) {
  auto *AllocatedBB = new BasicBlock(this);
  if (Before == nullptr) {
    BasicBlocks.push_back(AllocatedBB);
  } else {
    BasicBlocks.insert(Before->getIterator(), AllocatedBB);
  }
  return AllocatedBB;
}

BasicBlock const *Function::getEntryBasicBlock() const {
  if (BasicBlocks.empty()) return nullptr;
  return std::addressof(BasicBlocks.front());
}

BasicBlock *Function::getEntryBasicBlock() {
  if (BasicBlocks.empty()) return nullptr;
  return std::addressof(BasicBlocks.front());
}

bool Function::isVoidReturn() const { return Type.getResultTypes().empty(); }

bool Function::isMultiValueReturn() const {
  return Type.getResultTypes().size() > 1;
}

bool Function::isSingleValueReturn() const {
  return Type.getResultTypes().size() == 1;
}

void Function::erase(BasicBlock *BasicBlock_) {
  BasicBlocks.erase(BasicBlock_);
}

void Function::erase(Local *Local_) {
  assert(!Local_->isParameter());
  Locals.erase(Local_);
}

Local *Function::BuildLocal(bytecode::ValueType Type_, Local *Before) {
  auto *AllocatedLocal = new Local(this, Type_);
  if (Before == nullptr) {
    Locals.push_back(AllocatedLocal);
  } else {
    Locals.insert(Before->getIterator(), AllocatedLocal);
  }
  return AllocatedLocal;
}

Local::Local(IsParameterTag, Function *Parent_, bytecode::ValueType Type_)
    : Parent(Parent_), Type(Type_), IsParameter(true) {}

Local::Local(Function *Parent_, bytecode::ValueType Type_)
    : Parent(Parent_), Type(Type_), IsParameter(false) {}

Global::Global(Module *Parent_, bytecode::GlobalType Type_)
    : Parent(Parent_), Type(Type_) {}

Memory::Memory(Module *Parent_, bytecode::MemoryType Type_)
    : Parent(Parent_), Type(Type_) {}

Table::Table(Module *Parent_, bytecode::TableType Type_)
    : Parent(Parent_), Type(Type_) {}

Function *Module::BuildFunction(bytecode::FunctionType Type_) {
  auto *AllocatedFunction = new Function(this, std::move(Type_));
  Functions.push_back(AllocatedFunction);
  return AllocatedFunction;
}

Global *Module::BuildGlobal(bytecode::GlobalType Type_) {
  auto *AllocatedGlobal = new Global(this, Type_);
  Globals.push_back(AllocatedGlobal);
  return AllocatedGlobal;
}

Memory *Module::BuildMemory(bytecode::MemoryType Type_) {
  auto *AllocatedMemory = new Memory(this, Type_);
  Memories.push_back(AllocatedMemory);
  return AllocatedMemory;
}

Table *Module::BuildTable(bytecode::TableType Type_) {
  auto *AllocatedTable = new Table(this, Type_);
  Tables.push_back(AllocatedTable);
  return AllocatedTable;
}
} // namespace mir
