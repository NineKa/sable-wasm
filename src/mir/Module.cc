#include "Module.h"
#include "Function.h"

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/replace.hpp>

namespace mir {

/////////////////////////////// DataSegment ////////////////////////////////////
Data::Data(std::unique_ptr<InitializerExpr> Offset_)
    : ASTNode(ASTNodeKind::DataSegment), Parent(nullptr), Content(),
      Offset(std::move(Offset_)) {}

// clang-format off
InitializerExpr *Data::getOffset() const { return Offset.get(); }
void Data::setOffset(std::unique_ptr<InitializerExpr> Offset_)
{ Offset = std::move(Offset_); }
std::size_t Data::getSize() const { return Content.size(); }

std::span<const std::byte> Data::getContent() const { return Content; }
void Data::setContent(std::span<const std::byte> Content_)
{ Content = std::vector<std::byte>(Content_.begin(), Content_.end()); }
// clang-format on

Module *Data::getParent() const { return Parent; }
void Data::detach(ASTNode const *) noexcept { utility::unreachable(); }
bool Data::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::DataSegment;
}

/////////////////////////// ElementSegment /////////////////////////////////////
Element::Element(std::unique_ptr<InitializerExpr> Offset_)
    : ASTNode(ASTNodeKind::ElementSegment), Parent(nullptr),
      Offset(std::move(Offset_)) {}

Element::~Element() noexcept {
  for (auto *Function : Content)
    if (Function != nullptr) Function->remove_use(this);
}

InitializerExpr *Element::getOffset() const { return Offset.get(); }
void Element::setOffset(std::unique_ptr<InitializerExpr> Offset_) {
  Offset = std::move(Offset_);
}

std::span<Function *const> Element::getContent() const { return Content; }

void Element::setContent(std::span<Function *const> Content_) {
  for (auto *Function : Content)
    if (Function != nullptr) Function->remove_use(this);
  for (auto *Function : Content_)
    if (Function != nullptr) Function->add_use(this);
  Content = ranges::to<decltype(Content)>(Content_);
}

std::size_t Element::getSize() const { return Content.size(); }

Module *Element::getParent() const { return Parent; }

void Element::detach(ASTNode const *Node) noexcept {
  if (ranges::contains(Content, Node)) {
    ranges::replace(Content, Node, nullptr);
    return;
  }
  utility::unreachable();
}

bool Element::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::ElementSegment;
}

/////////////////////////////////// Global /////////////////////////////////////

Global::Global(bytecode::GlobalType Type_)
    : ASTNode(ASTNodeKind::Global), Parent(nullptr), Type(Type_),
      Initializer(nullptr) {}

bytecode::GlobalType const &Global::getType() const { return Type; }
bool Global::hasInitializer() const { return Initializer != nullptr; }
InitializerExpr *Global::getInitializer() const { return Initializer.get(); }
void Global::setInitializer(std::unique_ptr<InitializerExpr> Initializer_) {
  Initializer = std::move(Initializer_);
}

Module *Global::getParent() const { return Parent; }
void Global::detach(ASTNode const *) noexcept { utility::unreachable(); }
bool Global::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::Global;
}

/////////////////////////////////// Memory /////////////////////////////////////
Memory::Memory(bytecode::MemoryType Type_)
    : ASTNode(ASTNodeKind::Memory), Parent(nullptr), Type(Type_) {}

Memory::~Memory() noexcept {
  for (auto *Initializer : Initializers)
    if (Initializer != nullptr) Initializer->remove_use(this);
}

bytecode::MemoryType const &Memory::getType() const { return Type; }

Memory::initializer_iterator Memory::initializer_begin() const {
  return Initializers.begin();
}

Memory::initializer_iterator Memory::initializer_end() const {
  return Initializers.end();
}

void Memory::addInitializer(Data *DataSegment_) {
  if (DataSegment_ != nullptr) DataSegment_->add_use(this);
  Initializers.push_back(DataSegment_);
}

void Memory::setInitializers(std::span<Data *const> DataSegments_) {
  for (auto *DataSegment : Initializers)
    if (DataSegment != nullptr) DataSegment->remove_use(this);
  for (auto *DataSegment : DataSegments_)
    if (DataSegment != nullptr) DataSegment->add_use(this);
  Initializers = ranges::to<decltype(Initializers)>(DataSegments_);
}

bool Memory::hasInitializer() const { return !Initializers.empty(); }

Module *Memory::getParent() const { return Parent; }

void Memory::detach(ASTNode const *Node) noexcept {
  if (ranges::contains(Initializers, Node)) {
    ranges::replace(Initializers, Node, nullptr);
    return;
  }
  utility::unreachable();
}

bool Memory::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::Memory;
}

/////////////////////////////////// Table //////////////////////////////////////
Table::Table(bytecode::TableType Type_)
    : ASTNode(ASTNodeKind::Table), Parent(nullptr), Type(Type_) {}

Table::~Table() noexcept {
  for (auto *Initializer : Initializers)
    if (Initializer != nullptr) Initializer->remove_use(this);
}

bytecode::TableType const &Table::getType() const { return Type; }

Table::initializer_iterator Table::initializer_begin() const {
  return Initializers.begin();
}

Table::initializer_iterator Table::initializer_end() const {
  return Initializers.end();
}

void Table::addInitializer(Element *ElementSegment_) {
  if (ElementSegment_ != nullptr) ElementSegment_->add_use(this);
  Initializers.push_back(ElementSegment_);
}

void Table::setInitializers(std::span<Element *const> ElementSegments_) {
  for (auto *ElementSegment : Initializers)
    if (ElementSegment != nullptr) ElementSegment->remove_use(this);
  for (auto *ElementSegment : ElementSegments_)
    if (ElementSegment != nullptr) ElementSegment->add_use(this);
  Initializers = ranges::to<decltype(Initializers)>(ElementSegments_);
}

bool Table::hasInitializer() const { return !Initializers.empty(); }

Module *Table::getParent() const { return Parent; }

void Table::detach(ASTNode const *Node) noexcept {
  if (ranges::contains(Initializers, Node)) {
    ranges::replace(Initializers, Node, nullptr);
    return;
  }
  utility::unreachable();
}

bool Table::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::Table;
}

/////////////////////////////////// Module /////////////////////////////////////
Module::Module() : ASTNode(ASTNodeKind::Module) {}

Function *Module::BuildFunction(bytecode::FunctionType Type_) {
  auto *AllocatedFunction = new Function(std::move(Type_));
  getFunctions().push_back(AllocatedFunction);
  return AllocatedFunction;
}

Global *Module::BuildGlobal(bytecode::GlobalType Type_) {
  auto *AllocatedGlobal = new Global(Type_);
  getGlobals().push_back(AllocatedGlobal);
  return AllocatedGlobal;
}

Memory *Module::BuildMemory(bytecode::MemoryType Type_) {
  auto *AllocatedMemory = new Memory(Type_);
  getMemories().push_back(AllocatedMemory);
  return AllocatedMemory;
}

Table *Module::BuildTable(bytecode::TableType Type_) {
  auto *AllocatedTable = new Table(Type_);
  getTables().push_back(AllocatedTable);
  return AllocatedTable;
}

Data *Module::BuildDataSegment(std::unique_ptr<InitializerExpr> Offset_) {
  auto *AllocatedDataSegment = new Data(std::move(Offset_));
  getData().push_back(AllocatedDataSegment);
  return AllocatedDataSegment;
}

Element *Module::BuildElementSegment(std::unique_ptr<InitializerExpr> Offset_) {
  auto *AllocatedElementSegment = new Element(std::move(Offset_));
  getElements().push_back(AllocatedElementSegment);
  return AllocatedElementSegment;
}

// clang-format off
detail::IListAccessWrapper<Module, Function> Module::getFunctions()
{ return detail::IListAccessWrapper(this, Functions); }
detail::IListAccessWrapper<Module, Global> Module::getGlobals()
{ return detail::IListAccessWrapper(this, Globals); }
detail::IListAccessWrapper<Module, Memory> Module::getMemories()
{ return detail::IListAccessWrapper(this, Memories); }
detail::IListAccessWrapper<Module, Table> Module::getTables()
{ return detail::IListAccessWrapper(this, Tables); }
detail::IListAccessWrapper<Module, Data> Module::getData()
{ return detail::IListAccessWrapper(this, DataSegments); }
detail::IListAccessWrapper<Module, Element> Module::getElements()
{ return detail::IListAccessWrapper(this, ElementSegments); }

detail::IListConstAccessWrapper<Module, Function> Module::getFunctions() const
{ return detail::IListConstAccessWrapper(this, Functions); }
detail::IListConstAccessWrapper<Module, Global> Module::getGlobals() const
{ return detail::IListConstAccessWrapper(this, Globals); }
detail::IListConstAccessWrapper<Module, Memory> Module::getMemories() const
{ return detail::IListConstAccessWrapper(this, Memories); }
detail::IListConstAccessWrapper<Module, Table> Module::getTables() const
{ return detail::IListConstAccessWrapper(this, Tables); }
detail::IListConstAccessWrapper<Module, Data> Module::getData() const
{ return detail::IListConstAccessWrapper(this, DataSegments); }
detail::IListConstAccessWrapper<Module, Element> Module::getElements() const
{ return detail::IListConstAccessWrapper(this, ElementSegments); }

llvm::ilist<Function> Module::*Module::getSublistAccess(Function *)
{ return &Module::Functions; }
llvm::ilist<Global> Module::*Module::getSublistAccess(Global *)
{ return &Module::Globals; }
llvm::ilist<Memory> Module::*Module::getSublistAccess(Memory *)
{ return &Module::Memories; }
llvm::ilist<Table> Module::*Module::getSublistAccess(Table *)
{ return &Module::Tables; }
llvm::ilist<Data> Module::*Module::getSublistAccess(Data *)
{ return &Module::DataSegments; }
llvm::ilist<Element> Module::*Module::getSublistAccess(Element *)
{ return &Module::ElementSegments; }
// clang-format on

void Module::detach(ASTNode const *) noexcept { utility::unreachable(); }

bool Module::classof(ASTNode const *Node) {
  return Node->getASTNodeKind() == ASTNodeKind::Module;
}

} // namespace mir
