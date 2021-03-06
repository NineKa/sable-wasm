#include "Module.h"

namespace mir {
namespace detail {
bool ImportableEntity::isImported() const { return Import != nullptr; }

std::string_view ImportableEntity::getImportModuleName() const {
  return std::get<0>(*Import);
}

std::string_view ImportableEntity::getImportEntityName() const {
  return std::get<1>(*Import);
}

void ImportableEntity::setImport(
    std::string ModuleName, std::string EntityName) {
  if (ModuleName.empty() && EntityName.empty()) {
    Import = nullptr;
    return;
  }
  assert(!ModuleName.empty() && !EntityName.empty());
  Import = std::make_unique<ImportDescriptor>(
      std::move(ModuleName), std::move(EntityName));
}

bool ExportableEntity::isExported() const { return Export != nullptr; }

std::string_view ExportableEntity::getExportName() const { return *Export; }

void ExportableEntity::setExport(std::string EntityName) {
  if (EntityName.empty()) {
    Export = nullptr;
    return;
  }
  Export = std::make_unique<ExportDescriptor>(std::move(EntityName));
}

} // namespace detail

Function::Function(Module *Parent_, bytecode::FunctionType Type_)
    : ASTNode(ASTNodeKind::Function), Parent(Parent_), Type(std::move(Type_)) {}

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
    : ASTNode(ASTNodeKind::Local), Parent(Parent_), Type(Type_),
      IsParameter(true) {}

Local::Local(Function *Parent_, bytecode::ValueType Type_)
    : ASTNode(ASTNodeKind::Local), Parent(Parent_), Type(Type_),
      IsParameter(false) {}

Global::Global(Module *Parent_, bytecode::GlobalType Type_)
    : ASTNode(ASTNodeKind::Global), Parent(Parent_), Type(Type_) {}

Memory::Memory(Module *Parent_, bytecode::MemoryType Type_)
    : ASTNode(ASTNodeKind::Memory), Parent(Parent_), Type(Type_) {}

Table::Table(Module *Parent_, bytecode::TableType Type_)
    : ASTNode(ASTNodeKind::Table), Parent(Parent_), Type(Type_) {}

Module::Module() : ASTNode(ASTNodeKind::Module) {}

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
