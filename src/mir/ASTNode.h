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
  InitializerExpr
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
  using const_iterator = typename decltype(Uses)::const_iterator;
  const_iterator use_site_begin() const { return Uses.begin(); }
  const_iterator use_site_end() const { return Uses.end(); }

  auto getUsedSites() {
    return ranges::subrange(use_site_begin(), use_site_end());
  }

  auto getUsedSites() const {
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

enum class InitializerExprKind { Constant, GlobalGet };
class InitializerExpr : public ASTNode {
  InitializerExprKind Kind;

protected:
  explicit InitializerExpr(InitializerExprKind Kind_)
      : ASTNode(ASTNodeKind::InitializerExpr), Kind(Kind_) {}

public:
  InitializerExprKind getInitializerExprKind() const { return Kind; }
  static bool classof(ASTNode const *Node) {
    return Node->getASTNodeKind() == ASTNodeKind::InitializerExpr;
  }
};

template <typename T>
concept initializer_expr = ast_node<T> &&
    std::derived_from<T, InitializerExpr> &&requires() {
  { T::classof(std::declval<InitializerExpr const *>()) }
  ->std::convertible_to<bool>;
};

template <initializer_expr T> bool is_a(InitializerExpr const *Node) {
  return T::classof(Node);
}

template <initializer_expr T> T *dyn_cast(InitializerExpr *Node) {
  assert(is_a<T>(Node));
  return static_cast<T *>(Node);
}

template <initializer_expr T> T const *dyn_cast(InitializerExpr const *Node) {
  assert(is_a<T>(Node));
  return static_cast<T const *>(Node);
}

class Global;
namespace initializer {
class Constant : public InitializerExpr {
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
  static bool classof(InitializerExpr const *ConstExpr);
  static bool classof(ASTNode const *Node);
};

class GlobalGet : public InitializerExpr {
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
  static bool classof(InitializerExpr const *ConstExpr);
  static bool classof(ASTNode const *Node);
};
} // namespace initializer

template <typename Derived, typename RetType, bool Const = true>
class InitExprVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;
  template <initializer_expr T> RetType castAndCall(Ptr<InitializerExpr> Expr) {
    return derived()(dyn_cast<T>(Expr));
  }

public:
  RetType visit(Ptr<InitializerExpr> Expr) {
    using InitExprKind = InitializerExprKind;
    using namespace initializer;
    switch (Expr->getInitializerExprKind()) {
    case InitExprKind::Constant: return castAndCall<Constant>(Expr);
    case InitializerExprKind::GlobalGet: return castAndCall<GlobalGet>(Expr);
    default: utility::unreachable();
    }
  }
};

} // namespace mir

#endif
