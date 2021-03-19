#ifndef SABLE_INCLUDE_GUARD_MIR_MODULE
#define SABLE_INCLUDE_GUARD_MIR_MODULE

#include "../bytecode/Module.h"
#include "ASTNode.h"
#include "Instruction.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>

namespace mir {

namespace detail {
using ImportDescriptor = std::pair<std::string, std::string>;
using ExportDescriptor = std::string;
class ImportableEntity {
  std::unique_ptr<ImportDescriptor> Import = nullptr;

public:
  bool isImported() const;
  bool isDeclaration() const { return this->isImported(); }
  bool isDefinition() const { return !this->isImported(); }
  std::string_view getImportModuleName() const;
  std::string_view getImportEntityName() const;
  void setImport(std::string ModuleName, std::string EntityName);
};

class ExportableEntity {
  std::unique_ptr<ExportDescriptor> Export = nullptr;

public:
  bool isExported() const;
  std::string_view getExportName() const;
  void setExport(std::string EntityName);
};
} // namespace detail

class DataSegment :
    public ASTNode,
    public llvm::ilist_node_with_parent<DataSegment, Module> {
  Module *Parent;
  std::vector<std::byte> Content;
  std::unique_ptr<InitializerExpr> Offset;

public:
  DataSegment(
      Module *Parent_, std::unique_ptr<InitializerExpr> Offset_,
      std::span<std::byte const> Content_);

  Module *getParent() const { return Parent; }

  InitializerExpr *getOffset() const;
  void setOffset(std::unique_ptr<InitializerExpr> Offset_);

  std::span<std::byte const> getContent() const;
  void setContent(std::span<std::byte const> Content_);
  std::size_t getSize() const;

  void detach(ASTNode const *) noexcept override;
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Function;
  }
};

class ElementSegment :
    public ASTNode,
    public llvm::ilist_node_with_parent<ElementSegment, Module> {
  Module *Parent;
  std::vector<Function *> Content;
  std::unique_ptr<InitializerExpr> Offset;

public:
  ElementSegment(
      Module *Parent_, std::unique_ptr<InitializerExpr> Offset_,
      std::span<Function *const> Content_);
  ElementSegment(ElementSegment const &) = delete;
  ElementSegment(ElementSegment &&) noexcept = delete;
  ElementSegment &operator=(ElementSegment const &) = delete;
  ElementSegment &operator=(ElementSegment &&) noexcept = delete;
  ~ElementSegment() noexcept override;

  Module *getParent() const { return Parent; }

  InitializerExpr *getOffset() const;
  void setOffset(std::unique_ptr<InitializerExpr> Offset_);

  std::span<Function *const> getContent() const;
  void setContent(std::span<Function *const> Content_);
  std::size_t getSize() const;

  void detach(ASTNode const *) noexcept override;
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Function;
  }
};

class Function :
    public ASTNode,
    public detail::ImportableEntity,
    public detail::ExportableEntity,
    public llvm::ilist_node_with_parent<Function, Module> {
  Module *Parent;
  bytecode::FunctionType Type;
  llvm::ilist<BasicBlock> BasicBlocks;
  llvm::ilist<Local> Locals;

public:
  Function(Module *Parent_, bytecode::FunctionType Type_);

  Module *getParent() const { return Parent; }
  bytecode::FunctionType const &getType() const { return Type; }

  BasicBlock const *getEntryBasicBlock() const;
  BasicBlock *getEntryBasicBlock();

  using bb_iterator = decltype(BasicBlocks)::iterator;
  using bb_const_iterator = decltype(BasicBlocks)::const_iterator;
  bb_iterator basic_block_begin() { return BasicBlocks.begin(); }
  bb_iterator basic_block_end() { return BasicBlocks.end(); }
  bb_const_iterator basic_block_begin() const { return BasicBlocks.begin(); }
  bb_const_iterator basic_block_end() const { return BasicBlocks.end(); }

  auto getBasicBlocks() {
    return ranges::subrange(basic_block_begin(), basic_block_end());
  }
  auto getBasicBlocks() const {
    return ranges::subrange(basic_block_begin(), basic_block_end());
  }

  using local_iterator = decltype(Locals)::iterator;
  using local_const_iterator = decltype(Locals)::const_iterator;
  local_iterator local_begin() { return Locals.begin(); }
  local_iterator local_end() { return Locals.end(); }
  local_const_iterator local_begin() const { return Locals.begin(); }
  local_const_iterator local_end() const { return Locals.end(); }

  auto getLocals() { return ranges::subrange(local_begin(), local_end()); }
  auto getLocals() const {
    return ranges::subrange(local_begin(), local_end());
  }

  BasicBlock *BuildBasicBlock(BasicBlock *Before = nullptr);
  Local *BuildLocal(bytecode::ValueType Type_, Local *Before = nullptr);

  void erase(BasicBlock *BasicBlock_);
  void erase(Local *Local_);

  static llvm::ilist<BasicBlock> Function::*getSublistAccess(BasicBlock *) {
    return &Function::BasicBlocks;
  }
  static llvm::ilist<Local> Function::*getSublistAccess(Local *) {
    return &Function::Locals;
  }

  void detach(ASTNode const *) noexcept override { utility::unreachable(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Function;
  }
};

class Local :
    public ASTNode,
    public llvm::ilist_node_with_parent<Local, Function> {
  Function *Parent;
  bytecode::ValueType Type;
  bool IsParameter;

  friend class Function;
  struct IsParameterTag {};
  Local(IsParameterTag, Function *Parent_, bytecode::ValueType Type_);

public:
  Local(Function *Parent_, bytecode::ValueType Type_);
  Function *getParent() const { return Parent; }
  bytecode::ValueType const &getType() const { return Type; }
  bool isParameter() const { return IsParameter; }
  void detach(ASTNode const *) noexcept override { utility::unreachable(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Local;
  }
};

class Global :
    public ASTNode,
    public detail::ImportableEntity,
    public detail::ExportableEntity,
    public llvm::ilist_node_with_parent<Global, Module> {
  Module *Parent;
  bytecode::GlobalType Type;
  std::unique_ptr<InitializerExpr> Initializer;

public:
  Global(Module *Parent_, bytecode::GlobalType Type_);
  Module *getParent() const { return Parent; }
  bytecode::GlobalType const &getType() const { return Type; }
  InitializerExpr *getInitializer() const { return Initializer.get(); }
  void setInitializer(std::unique_ptr<InitializerExpr> Initializer_) {
    Initializer = std::move(Initializer_);
  }
  void detach(ASTNode const *) noexcept override { utility::unreachable(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Global;
  }
};

class Memory :
    public ASTNode,
    public detail::ImportableEntity,
    public detail::ExportableEntity,
    public llvm::ilist_node_with_parent<Memory, Module> {
  Module *Parent;
  bytecode::MemoryType Type;
  std::vector<DataSegment *> Initializers;

public:
  Memory(Module *Parent_, bytecode::MemoryType Type_);
  Memory(Memory const &) = delete;
  Memory(Memory &&) noexcept = delete;
  Memory &operator=(Memory const &) = delete;
  Memory &operator=(Memory &&) noexcept = delete;
  ~Memory() noexcept override;

  Module *getParent() const { return Parent; }
  bytecode::MemoryType const &getType() const { return Type; }

  using iterator = decltype(Initializers)::iterator;
  iterator initializer_begin() { return Initializers.begin(); }
  iterator initializer_end() { return Initializers.end(); }
  using const_iterator = decltype(Initializers)::const_iterator;
  const_iterator initializer_begin() const { return Initializers.begin(); }
  const_iterator initializer_end() const { return Initializers.end(); }
  auto getInitializers() const {
    return ranges::subrange(initializer_begin(), initializer_end());
  }

  void addInitializer(DataSegment *DataSegment_);
  void setInitializers(std::span<DataSegment *const> DataSegments_);

  void detach(ASTNode const *) noexcept override;
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Memory;
  }
};

class Table :
    public ASTNode,
    public detail::ImportableEntity,
    public detail::ExportableEntity,
    public llvm::ilist_node_with_parent<Table, Module> {
  Module *Parent;
  bytecode::TableType Type;
  std::vector<ElementSegment *> Initializers;

public:
  Table(Module *Parent_, bytecode::TableType Type_);
  Table(Table const &) = delete;
  Table(Table &&) = delete;
  Table &operator=(Table const &) = delete;
  Table &operator=(Table &&) noexcept = delete;
  ~Table() noexcept override;

  Module *getParent() const { return Parent; }
  bytecode::TableType const &getType() const { return Type; }

  using iterator = decltype(Initializers)::iterator;
  iterator initializer_begin() { return Initializers.begin(); }
  iterator initializer_end() { return Initializers.end(); }
  using const_iterator = decltype(Initializers)::const_iterator;
  const_iterator initializer_begin() const { return Initializers.begin(); }
  const_iterator initializer_end() const { return Initializers.end(); }
  auto getInitializers() const {
    return ranges::subrange(initializer_begin(), initializer_end());
  }

  void addInitializer(ElementSegment *ElementSegment_);
  void setInitializers(std::span<ElementSegment *const> ElementSegments_);

  void detach(ASTNode const *) noexcept override;
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Table;
  }
};

class Module : public ASTNode {
  llvm::ilist<Function> Functions;
  llvm::ilist<Global> Globals;
  llvm::ilist<Memory> Memories;
  llvm::ilist<Table> Tables;
  llvm::ilist<DataSegment> DataSegments;
  llvm::ilist<ElementSegment> ElementSegments;

public:
  Module();
  using func_iterator = decltype(Functions)::iterator;
  using func_const_iterator = decltype(Functions)::const_iterator;
  func_iterator function_begin() { return Functions.begin(); }
  func_iterator function_end() { return Functions.end(); }
  func_const_iterator function_begin() const { return Functions.begin(); }
  func_const_iterator function_end() const { return Functions.end(); }

  auto getFunctions() {
    return ranges::subrange(function_begin(), function_end());
  }
  auto getFunctions() const {
    return ranges::subrange(function_begin(), function_end());
  }

  using global_iterator = decltype(Globals)::iterator;
  using global_const_iterator = decltype(Globals)::const_iterator;
  global_iterator global_begin() { return Globals.begin(); }
  global_iterator global_end() { return Globals.end(); }
  global_const_iterator global_begin() const { return Globals.begin(); }
  global_const_iterator global_end() const { return Globals.end(); }

  auto getGlobals() { return ranges::subrange(global_begin(), global_end()); }
  auto getGlobals() const {
    return ranges::subrange(global_begin(), global_end());
  }

  using mem_iterator = decltype(Memories)::iterator;
  using mem_const_iterator = decltype(Memories)::const_iterator;
  mem_iterator memory_begin() { return Memories.begin(); }
  mem_iterator memory_end() { return Memories.end(); }
  mem_const_iterator memory_begin() const { return Memories.begin(); }
  mem_const_iterator memory_end() const { return Memories.end(); }

  auto getMemories() { return ranges::subrange(memory_begin(), memory_end()); }
  auto getMemories() const {
    return ranges::subrange(memory_begin(), memory_end());
  }

  using table_iterator = decltype(Tables)::iterator;
  using table_const_iterator = decltype(Tables)::const_iterator;
  table_iterator table_begin() { return Tables.begin(); }
  table_iterator table_end() { return Tables.end(); }
  table_const_iterator table_begin() const { return Tables.begin(); }
  table_const_iterator table_end() const { return Tables.end(); }

  auto getTables() { return ranges::subrange(table_begin(), table_end()); }
  auto getTables() const {
    return ranges::subrange(table_begin(), table_end());
  }

  using data_segment_iterator = decltype(DataSegments)::iterator;
  using data_segment_const_iterator = decltype(DataSegments)::const_iterator;
  data_segment_iterator data_begin() { return DataSegments.begin(); }
  data_segment_iterator data_end() { return DataSegments.end(); }
  data_segment_const_iterator data_begin() const {
    return DataSegments.begin();
  }
  data_segment_const_iterator data_end() const { return DataSegments.end(); }

  auto getData() { return ranges::subrange(data_begin(), data_end()); }
  auto getData() const { return ranges::subrange(data_begin(), data_end()); }

  using element_segment_iterator = decltype(ElementSegments)::iterator;
  using element_segment_const_iterator =
      decltype(ElementSegments)::const_iterator;
  element_segment_iterator element_begin() { return ElementSegments.begin(); }
  element_segment_iterator element_end() { return ElementSegments.end(); }
  element_segment_const_iterator element_begin() const {
    return ElementSegments.begin();
  }
  element_segment_const_iterator element_end() const {
    return ElementSegments.end();
  }

  auto getElements() {
    return ranges::subrange(element_begin(), element_end());
  }
  auto getElements() const {
    return ranges::subrange(element_begin(), element_end());
  }

  Function *BuildFunction(bytecode::FunctionType Type_);
  Global *BuildGlobal(bytecode::GlobalType Type_);
  Memory *BuildMemory(bytecode::MemoryType Type_);
  Table *BuildTable(bytecode::TableType Type_);
  DataSegment *BuildDataSegment(
      std::unique_ptr<InitializerExpr> Offset_,
      std::span<std::byte const> Content_);
  ElementSegment *BuildElementSegment(
      std::unique_ptr<InitializerExpr> Offset_,
      std::span<Function *const> Content_);

  static llvm::ilist<Function> Module::*getSublistAccess(Function *) {
    return &Module::Functions;
  }
  static llvm::ilist<Global> Module::*getSublistAccess(Global *) {
    return &Module::Globals;
  }
  static llvm::ilist<Memory> Module::*getSublistAccess(Memory *) {
    return &Module::Memories;
  }
  static llvm::ilist<Table> Module::*getSublistAccess(Table *) {
    return &Module::Tables;
  }
  static llvm::ilist<DataSegment> Module::*getSublistAccess(DataSegment *) {
    return &Module::DataSegments;
  }
  static llvm::ilist<ElementSegment> Module::*
  getSublistAccess(ElementSegment *) {
    return &Module::ElementSegments;
  }

  void detach(ASTNode const *) noexcept override { utility::unreachable(); }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::Module;
  }
};
} // namespace mir

#endif
