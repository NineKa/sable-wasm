#ifndef SABLE_INCLUDE_GUARD_MIR_ASTNODE
#define SABLE_INCLUDE_GUARD_MIR_ASTNODE

#include <cstdint>
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
  Module
};

class ASTNode {
  ASTNodeKind Kind;
  std::string Name;

public:
  ASTNode(ASTNodeKind Kind_, std::string Name_)
      : Kind(Kind_), Name(std::move(Name_)) {}
  ASTNode(ASTNode const &) = delete;
  ASTNode(ASTNode &&) noexcept = delete;
  ASTNode &operator=(ASTNode const &) = delete;
  ASTNode &operator=(ASTNode &&) noexcept = delete;
  virtual ~ASTNode() noexcept = default;

  std::string_view getName() const { return Name; }
  void setName(std::string Name_) { Name = std::move(Name_); }
  bool hasName() const { return !Name.empty(); }

  ASTNodeKind getASTNodeKind() const { return Kind; }
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

template <ast_node T> T const *dyn_cast(ASTNode *Node) {
  assert(is_a<T>(Node));
  return static_cast<T const *>(Node);
}
} // namespace mir

#endif