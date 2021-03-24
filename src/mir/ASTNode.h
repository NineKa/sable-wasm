#ifndef SABLE_INCLUDE_GUARD_MIR_ASTNODE
#define SABLE_INCLUDE_GUARD_MIR_ASTNODE

#include "../bytecode/Type.h"

#include <llvm/ADT/ilist.h>
#include <range/v3/view/transform.hpp>

#include <cstdint>
#include <forward_list>
#include <string>
#include <variant>

namespace mir {
class ASTNode;
enum class ASTNodeKind : std::uint8_t {
  Instruction,
  BasicBlock,
  Local,
  Function,
  Memory,
  Table,
  Global,
  Module,
  DataSegment,
  ElementSegment,
  InitializerExpr
};

class ASTNode {
  ASTNodeKind Kind;
  std::string Name;
  std::forward_list<ASTNode *> Uses;

public:
  ASTNode(ASTNodeKind Kind_);
  ASTNode(ASTNode const &) = delete;
  ASTNode(ASTNode &&) noexcept = delete;
  ASTNode &operator=(ASTNode const &) = delete;
  ASTNode &operator=(ASTNode &&) noexcept = delete;
  virtual ~ASTNode() noexcept;

  std::string_view getName() const;
  void setName(std::string Name_);
  bool hasName() const;

  ASTNodeKind getASTNodeKind() const;

  using use_site_iterator = decltype(Uses)::const_iterator;
  use_site_iterator use_site_begin() const;
  use_site_iterator use_site_end() const;

  auto getUsedSites() const {
    auto Begin = use_site_begin();
    auto End = use_site_end();
    return ranges::make_subrange(Begin, End);
  }

  void add_use(ASTNode *Referrer);
  void remove_use(ASTNode *Referrer);
  virtual void replace(ASTNode const *Old, ASTNode *New) noexcept = 0;
};

class ImportableEntity {
  using ImportDescriptor = std::pair<std::string, std::string>;
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
  using ExportDescriptor = std::string;
  std::unique_ptr<ExportDescriptor> Export = nullptr;

public:
  bool isExported() const;
  std::string_view getExportName() const;
  void setExport(std::string EntityName);
};

template <typename T>
concept ast_node = std::derived_from<T, ASTNode> &&requires() {
  { T::classof(std::declval<ASTNode const *>()) }
  ->std::convertible_to<bool>;
};

template <ast_node T> bool is_a(ASTNode const *Node) {
  return T::classof(Node);
}

template <ast_node T> bool is_a(ASTNode const &Node) {
  return T::classof(std::addressof(Node));
}

template <ast_node T> T *dyn_cast(ASTNode *Node) {
  if (Node == nullptr) return nullptr;
  assert(is_a<T>(Node));
  return static_cast<T *>(Node);
}

template <ast_node T> T &dyn_cast(ASTNode &Node) {
  assert(is_a<T>(Node));
  return static_cast<T &>(Node);
}

template <ast_node T> T const *dyn_cast(ASTNode const *Node) {
  if (Node == nullptr) return nullptr;
  assert(is_a<T>(Node));
  return static_cast<T const *>(Node);
}

template <ast_node T> T const &dyn_cast(ASTNode const &Node) {
  assert(is_a<T>(Node));
  return static_cast<T const &>(Node);
}

namespace detail {
template <typename ParentType, typename IListElementType>
class IListAccessWrapper {
  ParentType *Parent;
  llvm::ilist<IListElementType> &List;

public:
  IListAccessWrapper(ParentType *Parent_, llvm::ilist<IListElementType> &List_)
      : Parent(Parent_), List(List_) {}

  using iterator = typename llvm::ilist<IListElementType>::iterator;
  using const_iterator = typename llvm::ilist<IListElementType>::const_iterator;
  iterator begin() { return List.begin(); }
  iterator end() { return List.end(); }
  const_iterator begin() const { return List.begin(); }
  const_iterator end() const { return List.end(); }
  auto asView() { return ranges::make_subrange(begin(), end()); }
  auto asView() const { return ranges::make_subrange(begin(), end()); }

  void erase(IListElementType *ElementPtr) {
    assert(ElementPtr->Parent == Parent);
    List.erase(ElementPtr);
  }

  IListElementType *remove(IListElementType *ElementPtr) {
    assert(ElementPtr->Parent == Parent);
    auto *Result = List.remove(ElementPtr);
    ElementPtr->Parent = nullptr;
    return Result;
  }

  void push_back(IListElementType *ElementPtr) {
    assert(ElementPtr->Parent == nullptr);
    ElementPtr->Parent = Parent;
    List.push_back(ElementPtr);
  }

  iterator insert(iterator Pos, IListElementType *ElementPtr) {
    assert(ElementPtr->Parent == nullptr);
    ElementPtr->Parent = Parent;
    return List.insert(Pos, ElementPtr);
  }

  iterator insertAfter(iterator Pos, IListElementType *ElementPtr) {
    assert(ElementPtr->Parent == nullptr);
    ElementPtr->Parent = Parent;
    return List.insertAt(Pos, ElementPtr);
  }

  std::size_t size() const { return List.size(); }
};

template <typename ParentType, typename IListElementType>
class IListConstAccessWrapper {
  ParentType const *Parent;
  llvm::ilist<IListElementType> const &List;

public:
  IListConstAccessWrapper(
      ParentType const *Parent_, llvm::ilist<IListElementType> const &List_)
      : Parent(Parent_), List(List_) {}

  using const_iterator = typename llvm::ilist<IListElementType>::const_iterator;
  const_iterator begin() const { return List.begin(); }
  const_iterator end() const { return List.end(); }
  auto asView() const { return ranges::make_subrange(begin(), end()); }
  std::size_t size() const { return List.size(); }
};
} // namespace detail
} // namespace mir

#endif
