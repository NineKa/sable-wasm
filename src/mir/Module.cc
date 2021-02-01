#include "Module.h"

namespace mir {
Function::Function(Module *Parent_, bytecode::FunctionType Type_)
    : Parent(Parent_), Type(std::move(Type_)) {
  for (auto const &ValueType : Type.getResultTypes()) {
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

} // namespace mir
