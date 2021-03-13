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
/////////////////////////////// DataSegment ////////////////////////////////////
DataSegment::DataSegment(
    Module *Parent_, std::variant<std::uint32_t, Global *> Offset_,
    std::span<std::byte const> Content_)
    : ASTNode(ASTNodeKind::DataSegment), Parent(Parent_),
      Content(Content_.begin(), Content_.end()) {
  setOffset(Offset_);
}

bool DataSegment::isOffsettedByConstant() const {
  return std::holds_alternative<std::uint32_t>(Offset);
}

bool DataSegment::isOffsettedByGlobalValue() const {
  return std::holds_alternative<Global *>(Offset);
}

std::int32_t DataSegment::getConstantOffset() const {
  return std::get<std::uint32_t>(Offset);
}

Global *DataSegment::getGlobalValueOffset() const {
  return std::get<Global *>(Offset);
}

void DataSegment::setOffset(std::variant<std::uint32_t, Global *> Offset_) {
  if (isOffsettedByGlobalValue() && (getGlobalValueOffset() != nullptr))
    getGlobalValueOffset()->remove_use(this);
  Offset = Offset_;
  if (isOffsettedByGlobalValue() && (getGlobalValueOffset() != nullptr))
    getGlobalValueOffset()->add_use(this);
}

std::size_t DataSegment::getSize() const { return Content.size(); }
std::span<const std::byte> DataSegment::getContent() const { return Content; }
void DataSegment::setContent(std::span<const std::byte> Content_) {
  Content = std::vector<std::byte>(Content_.begin(), Content_.end());
}

void DataSegment::detach(ASTNode const *Node) noexcept {
  if (isOffsettedByGlobalValue() && (getGlobalValueOffset() == Node)) {
    setOffset(static_cast<Global *>(nullptr));
  }
  SABLE_UNREACHABLE();
}
/////////////////////////// ElementSegment /////////////////////////////////////

///////////////////////////////// Function /////////////////////////////////////
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

////////////////////////////////// Local ///////////////////////////////////////
Local::Local(IsParameterTag, Function *Parent_, bytecode::ValueType Type_)
    : ASTNode(ASTNodeKind::Local), Parent(Parent_), Type(Type_),
      IsParameter(true) {}

Local::Local(Function *Parent_, bytecode::ValueType Type_)
    : ASTNode(ASTNodeKind::Local), Parent(Parent_), Type(Type_),
      IsParameter(false) {}

////////////////////////////////// Global //////////////////////////////////////
Global::Global(Module *Parent_, bytecode::GlobalType Type_)
    : ASTNode(ASTNodeKind::Global), Parent(Parent_), Type(Type_) {}

/////////////////////////////////// Memory /////////////////////////////////////
Memory::Memory(Module *Parent_, bytecode::MemoryType Type_)
    : ASTNode(ASTNodeKind::Memory), Parent(Parent_), Type(Type_) {}

/////////////////////////////////// Table //////////////////////////////////////
Table::Table(Module *Parent_, bytecode::TableType Type_)
    : ASTNode(ASTNodeKind::Table), Parent(Parent_), Type(Type_) {}

/////////////////////////////////// Module /////////////////////////////////////
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
