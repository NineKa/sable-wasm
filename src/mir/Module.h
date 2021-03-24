#ifndef SABLE_INCLUDE_GUARD_MIR_MODULE
#define SABLE_INCLUDE_GUARD_MIR_MODULE

#include "../bytecode/Module.h"
#include "ASTNode.h"
#include "BasicBlock.h"
#include "InitializerExpr.h"
#include "Instruction.h"

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>

#include <range/v3/view/subrange.hpp>

namespace mir {

class Module;
class Data : public ASTNode, public llvm::ilist_node_with_parent<Data, Module> {
  friend class detail::IListAccessWrapper<Module, Data>;
  friend class detail::IListConstAccessWrapper<Module, Data>;
  Module *Parent;
  std::vector<std::byte> Content;
  std::unique_ptr<InitializerExpr> Offset;

public:
  explicit Data(std::unique_ptr<InitializerExpr> Offset_);

  InitializerExpr *getOffset() const;
  void setOffset(std::unique_ptr<InitializerExpr> Offset_);

  std::span<std::byte const> getContent() const;
  void setContent(std::span<std::byte const> Content_);
  std::size_t getSize() const;

  Module *getParent() const;
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(ASTNode const *Node);
};

class Element :
    public ASTNode,
    public llvm::ilist_node_with_parent<Element, Module> {
  friend class detail::IListAccessWrapper<Module, Element>;
  friend class detail::IListConstAccessWrapper<Module, Element>;
  Module *Parent;
  std::vector<Function *> Content;
  std::unique_ptr<InitializerExpr> Offset;

public:
  explicit Element(std::unique_ptr<InitializerExpr> Offset_);
  Element(Element const &) = delete;
  Element(Element &&) noexcept = delete;
  Element &operator=(Element const &) = delete;
  Element &operator=(Element &&) noexcept = delete;
  ~Element() noexcept override;

  InitializerExpr *getOffset() const;
  void setOffset(std::unique_ptr<InitializerExpr> Offset_);

  std::span<Function *const> getContent() const;
  void setContent(std::span<Function *const> Content_);
  Function *getEntry(std::size_t Index) const;
  void setEntry(std::size_t Index, Function *Function_);
  std::size_t getSize() const;

  Module *getParent() const;
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(ASTNode const *Node);
};

class Global :
    public ASTNode,
    public ImportableEntity,
    public ExportableEntity,
    public llvm::ilist_node_with_parent<Global, Module> {
  friend class detail::IListAccessWrapper<Module, Global>;
  friend class detail::IListConstAccessWrapper<Module, Global>;
  Module *Parent;
  bytecode::GlobalType Type;
  std::unique_ptr<InitializerExpr> Initializer;

public:
  explicit Global(bytecode::GlobalType Type_);

  bytecode::GlobalType const &getType() const;
  bool hasInitializer() const;
  InitializerExpr *getInitializer() const;
  void setInitializer(std::unique_ptr<InitializerExpr> Initializer_);

  Module *getParent() const;
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(ASTNode const *Node);
};

class Memory :
    public ASTNode,
    public ImportableEntity,
    public ExportableEntity,
    public llvm::ilist_node_with_parent<Memory, Module> {
  friend class detail::IListAccessWrapper<Module, Memory>;
  friend class detail::IListConstAccessWrapper<Module, Memory>;
  Module *Parent;
  bytecode::MemoryType Type;
  std::vector<Data *> Initializers;

public:
  explicit Memory(bytecode::MemoryType Type_);
  Memory(Memory const &) = delete;
  Memory(Memory &&) noexcept = delete;
  Memory &operator=(Memory const &) = delete;
  Memory &operator=(Memory &&) noexcept = delete;
  ~Memory() noexcept override;

  bytecode::MemoryType const &getType() const;

  using initializer_iterator = decltype(Initializers)::const_iterator;
  initializer_iterator initializer_begin() const;
  initializer_iterator initializer_end() const;

  auto getInitializers() const {
    return ranges::make_subrange(initializer_begin(), initializer_end());
  }

  void addInitializer(Data *DataSegment_);
  void setInitializers(std::span<Data *const> DataSegments_);
  bool hasInitializer() const;

  std::size_t getNumInitializers() const;
  Data *getInitializer(std::size_t Index) const;
  void setInitializer(std::size_t Index, Data *DataSegment_);

  Module *getParent() const;
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(ASTNode const *Node);
};

class Table :
    public ASTNode,
    public ImportableEntity,
    public ExportableEntity,
    public llvm::ilist_node_with_parent<Table, Module> {
  friend class detail::IListAccessWrapper<Module, Table>;
  friend class detail::IListConstAccessWrapper<Module, Table>;
  Module *Parent;
  bytecode::TableType Type;
  std::vector<Element *> Initializers;

public:
  explicit Table(bytecode::TableType Type_);
  Table(Table const &) = delete;
  Table(Table &&) = delete;
  Table &operator=(Table const &) = delete;
  Table &operator=(Table &&) noexcept = delete;
  ~Table() noexcept override;

  bytecode::TableType const &getType() const;

  using initializer_iterator = decltype(Initializers)::const_iterator;
  initializer_iterator initializer_begin() const;
  initializer_iterator initializer_end() const;

  auto getInitializers() const {
    return ranges::subrange(initializer_begin(), initializer_end());
  }

  void addInitializer(Element *ElementSegment_);
  void setInitializers(std::span<Element *const> ElementSegments_);
  bool hasInitializer() const;

  std::size_t getNumInitializers() const;
  Element *getInitializer(std::size_t Index) const;
  void setInitializer(std::size_t Index, Element *ElementSegment_);

  Module *getParent() const;
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(ASTNode const *Node);
};

class Module : public ASTNode {
  llvm::ilist<Function> Functions;
  llvm::ilist<Global> Globals;
  llvm::ilist<Memory> Memories;
  llvm::ilist<Table> Tables;
  llvm::ilist<Data> DataSegments;
  llvm::ilist<Element> ElementSegments;

public:
  Module();

  Function *BuildFunction(bytecode::FunctionType Type_);
  Global *BuildGlobal(bytecode::GlobalType Type_);
  Memory *BuildMemory(bytecode::MemoryType Type_);
  Table *BuildTable(bytecode::TableType Type_);
  Data *BuildDataSegment(std::unique_ptr<InitializerExpr> Offset_);
  Element *BuildElementSegment(std::unique_ptr<InitializerExpr> Offset_);

  detail::IListAccessWrapper<Module, Function> getFunctions();
  detail::IListAccessWrapper<Module, Global> getGlobals();
  detail::IListAccessWrapper<Module, Memory> getMemories();
  detail::IListAccessWrapper<Module, Table> getTables();
  detail::IListAccessWrapper<Module, Data> getData();
  detail::IListAccessWrapper<Module, Element> getElements();

  detail::IListConstAccessWrapper<Module, Function> getFunctions() const;
  detail::IListConstAccessWrapper<Module, Global> getGlobals() const;
  detail::IListConstAccessWrapper<Module, Memory> getMemories() const;
  detail::IListConstAccessWrapper<Module, Table> getTables() const;
  detail::IListConstAccessWrapper<Module, Data> getData() const;
  detail::IListConstAccessWrapper<Module, Element> getElements() const;

  static llvm::ilist<Function> Module::*getSublistAccess(Function *);
  static llvm::ilist<Global> Module::*getSublistAccess(Global *);
  static llvm::ilist<Memory> Module::*getSublistAccess(Memory *);
  static llvm::ilist<Table> Module::*getSublistAccess(Table *);
  static llvm::ilist<Data> Module::*getSublistAccess(Data *);
  static llvm::ilist<Element> Module::*getSublistAccess(Element *);
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
  static bool classof(ASTNode const *Node);
};
} // namespace mir

#endif
