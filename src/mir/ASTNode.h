#ifndef SABLE_INCLUDE_GUARD_MIR_ASTNODE
#define SABLE_INCLUDE_GUARD_MIR_ASTNODE

#include "../bytecode/Type.h"

#include <cstdint>
#include <forward_list>
#include <string>
#include <variant>

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
  ElementSegment,
  ConstantExpr
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

  auto getUsedSites() {
    return ranges::subrange(use_site_begin(), use_site_end());
  }

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

enum class ConstantExprKind { Constant, GlobalGet };
class ConstantExpr : public ASTNode {
  ConstantExprKind Kind;

protected:
  explicit ConstantExpr(ConstantExprKind Kind_)
      : ASTNode(ASTNodeKind::ConstantExpr), Kind(Kind_) {}

public:
  ConstantExprKind getConstantExprKind() const { return Kind; }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::ConstantExpr;
  }
};

template <typename T>
concept constant_epxr = ast_node<T> &&
    std::derived_from<T, ConstantExpr> &&requires() {
  { T::classof(std::declval<ConstantExpr const *>()) }
  ->std::convertible_to<bool>;
};

template <constant_epxr T> bool is_a(ConstantExpr const *Node) {
  return T::classof(Node);
}

template <constant_epxr T> T *dyn_cast(ConstantExpr *Node) {
  assert(is_a<T>(Node));
  return static_cast<T *>(Node);
}

template <constant_epxr T> T const *dyn_cast(ConstantExpr const *Node) {
  assert(is_a<T>(Node));
  return static_cast<T const *>(Node);
}

class Global;
namespace constant_expr {
class Constant : public ConstantExpr {
  std::variant<std::int32_t, std::int64_t, float, double> Storage;

public:
  Constant(std::int32_t Value);
  Constant(std::int64_t Value);
  Constant(float Value);
  Constant(double Value);
  std::int32_t &asI32();
  std::int64_t &asI64();
  float &asF32();
  double &asF64();
  std::int32_t asI32() const;
  std::int64_t asI64() const;
  float asF32() const;
  double asF64() const;
  bytecode::ValueType getValueType() const;
  void detach(ASTNode const *) noexcept override;
  static bool classof(ConstantExpr const *ConstExpr);
  static bool classof(ASTNode const *Node);
};

class GlobalGet : public ConstantExpr {
  Global *GlobalValue;

public:
  GlobalGet(Global *GlobalValue_);
  GlobalGet(GlobalGet const &) = delete;
  GlobalGet(GlobalGet &&) noexcept = delete;
  GlobalGet &operator=(GlobalGet const &) = delete;
  GlobalGet &operator=(GlobalGet &&) noexcept = delete;
  ~GlobalGet() noexcept override;
  Global *getGlobalValue() const;
  void setGlobalValue(Global *GlobalValue_);
  void detach(ASTNode const *) noexcept override;
  static bool classof(ConstantExpr const *ConstExpr);
  static bool classof(ASTNode const *Node);
};
} // namespace constant_expr
} // namespace mir

#endif
