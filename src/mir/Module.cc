#include "Module.h"

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/replace.hpp>

namespace mir {
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

/////////////////////////////// DataSegment ////////////////////////////////////
DataSegment::DataSegment(
    Module *Parent_, std::unique_ptr<InitializerExpr> Offset_,
    std::span<std::byte const> Content_)
    : ASTNode(ASTNodeKind::DataSegment), Parent(Parent_),
      Content(Content_.begin(), Content_.end()), Offset(std::move(Offset_)) {}

InitializerExpr *DataSegment::getOffset() const { return Offset.get(); }
void DataSegment::setOffset(std::unique_ptr<InitializerExpr> Offset_) {
  Offset = std::move(Offset_);
}

std::size_t DataSegment::getSize() const { return Content.size(); }
std::span<const std::byte> DataSegment::getContent() const { return Content; }
void DataSegment::setContent(std::span<const std::byte> Content_) {
  Content = std::vector<std::byte>(Content_.begin(), Content_.end());
}

void DataSegment::detach(ASTNode const *) noexcept { utility::unreachable(); }
/////////////////////////// ElementSegment /////////////////////////////////////
ElementSegment::ElementSegment(
    Module *Parent_, std::unique_ptr<InitializerExpr> Offset_,
    std::span<Function *const> Content_)
    : ASTNode(ASTNodeKind::ElementSegment), Parent(Parent_),
      Offset(std::move(Offset_)) {
  setContent(Content_);
}

ElementSegment::~ElementSegment() noexcept {
  for (auto *Function : Content)
    if (Function != nullptr) Function->remove_use(this);
}

InitializerExpr *ElementSegment::getOffset() const { return Offset.get(); }
void ElementSegment::setOffset(std::unique_ptr<InitializerExpr> Offset_) {
  Offset = std::move(Offset_);
}

std::span<Function *const> ElementSegment::getContent() const {
  return Content;
}

void ElementSegment::setContent(std::span<Function *const> Content_) {
  for (auto *Function : Content)
    if (Function != nullptr) Function->remove_use(this);
  for (auto *Function : Content_)
    if (Function != nullptr) Function->add_use(this);
  Content = ranges::to<decltype(Content)>(Content_);
}

std::size_t ElementSegment::getSize() const { return Content.size(); }

void ElementSegment::detach(ASTNode const *Node) noexcept {
  if (ranges::contains(Content, Node)) {
    ranges::replace(Content, Node, nullptr);
    return;
  }
  utility::unreachable();
}

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

Memory::~Memory() noexcept {
  for (auto *Initializer : Initializers)
    if (Initializer != nullptr) Initializer->remove_use(this);
}

void Memory::addInitializer(DataSegment *DataSegment_) {
  if (DataSegment_ != nullptr) DataSegment_->add_use(this);
  Initializers.push_back(DataSegment_);
}

void Memory::setInitializers(std::span<DataSegment *const> DataSegments_) {
  for (auto *DataSegment : Initializers)
    if (DataSegment != nullptr) DataSegment->remove_use(this);
  for (auto *DataSegment : DataSegments_)
    if (DataSegment != nullptr) DataSegment->add_use(this);
  Initializers = ranges::to<decltype(Initializers)>(DataSegments_);
}

void Memory::detach(ASTNode const *Node) noexcept {
  if (ranges::contains(Initializers, Node)) {
    ranges::replace(Initializers, Node, nullptr);
    return;
  }
  utility::unreachable();
}

/////////////////////////////////// Table //////////////////////////////////////
Table::Table(Module *Parent_, bytecode::TableType Type_)
    : ASTNode(ASTNodeKind::Table), Parent(Parent_), Type(Type_) {}

Table::~Table() noexcept {
  for (auto *Initializer : Initializers)
    if (Initializer != nullptr) Initializer->remove_use(this);
}

void Table::addInitializer(ElementSegment *ElementSegment_) {
  if (ElementSegment_ != nullptr) ElementSegment_->add_use(this);
  Initializers.push_back(ElementSegment_);
}

void Table::setInitializers(std::span<ElementSegment *const> ElementSegments_) {
  for (auto *ElementSegment : Initializers)
    if (ElementSegment != nullptr) ElementSegment->remove_use(this);
  for (auto *ElementSegment : ElementSegments_)
    if (ElementSegment != nullptr) ElementSegment->add_use(this);
  Initializers = ranges::to<decltype(Initializers)>(ElementSegments_);
}

void Table::detach(ASTNode const *Node) noexcept {
  if (ranges::contains(Initializers, Node)) {
    ranges::replace(Initializers, Node, nullptr);
    return;
  }
  utility::unreachable();
}

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

DataSegment *Module::BuildDataSegment(
    std::unique_ptr<InitializerExpr> Offset_,
    std::span<std::byte const> Content_) {
  auto *AllocatedDataSegment =
      new DataSegment(this, std::move(Offset_), Content_);
  DataSegments.push_back(AllocatedDataSegment);
  return AllocatedDataSegment;
}

ElementSegment *Module::BuildElementSegment(
    std::unique_ptr<InitializerExpr> Offset_,
    std::span<Function *const> Content_) {
  auto *AllocatedElementSegment =
      new ElementSegment(this, std::move(Offset_), Content_);
  ElementSegments.push_back(AllocatedElementSegment);
  return AllocatedElementSegment;
}

} // namespace mir
