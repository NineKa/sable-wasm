#ifndef SABLE_INCLUDE_GUARD_MIR_ASTNODE
#define SABLE_INCLUDE_GUARD_MIR_ASTNODE

#include <cstdint>
#include <forward_list>
#include <string>

namespace mir {

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
  ElementSegment
};

class ASTNode {
  ASTNodeKind Kind;
  std::string Name;
  std::forward_list<ASTNode *> Uses;

public:
  ASTNode(ASTNodeKind Kind_) : Kind(Kind_), Name() {}
  ASTNode(ASTNode const &) = delete;
  ASTNode(ASTNode &&) noexcept = delete;
  ASTNode &operator=(ASTNode const &) = delete;
  ASTNode &operator=(ASTNode &&) noexcept = delete;
  virtual ~ASTNode() noexcept {
    for (auto *U : Uses) U->detach(this);
  }

  std::string_view getName() const { return Name; }
  void setName(std::string Name_) { Name = std::move(Name_); }
  bool hasName() const { return !Name.empty(); }

  ASTNodeKind getASTNodeKind() const { return Kind; }

  using iterator = typename decltype(Uses)::iterator;
  iterator use_site_begin() { return Uses.begin(); }
  iterator use_site_end() { return Uses.end(); }

  void add_use(ASTNode *Referrer) { Uses.push_front(Referrer); }
  void remove_use(ASTNode *Referrer) { std::erase(Uses, Referrer); }

  virtual void detach(ASTNode const *) noexcept = 0;
};

template <typename T>
concept ast_node = std::derived_from<T, ASTNode> &&requires() {
  { T::classof(std::declval<ASTNode const *>()) }
  ->std::convertible_to<bool>;
};

template <ast_node T> bool is_a(ASTNode const *Node) {
  return T::classof(Node);
}

template <ast_node T> T *dyn_cast(ASTNode *Node) {
  assert(is_a<T>(Node));
  return static_cast<T *>(Node);
}

template <ast_node T> T const *dyn_cast(ASTNode const *Node) {
  assert(is_a<T>(Node));
  return static_cast<T const *>(Node);
}
} // namespace mir

#endif
