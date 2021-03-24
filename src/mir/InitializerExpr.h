#ifndef SABLE_INCLUDE_GUARD_MIR_INITIALIZER_EXPR
#define SABLE_INCLUDE_GUARD_MIR_INITIALIZER_EXPR

#include "ASTNode.h"

namespace mir {
enum class InitializerExprKind { Constant, GlobalGet };
class InitializerExpr : public ASTNode {
  InitializerExprKind Kind;

protected:
  explicit InitializerExpr(InitializerExprKind Kind_);

public:
  InitializerExprKind getInitializerExprKind() const;
  static bool classof(ASTNode const *Node);
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
  if (Node == nullptr) return nullptr;
  assert(is_a<T>(Node));
  return static_cast<T *>(Node);
}

template <initializer_expr T> T const *dyn_cast(InitializerExpr const *Node) {
  if (Node == nullptr) return nullptr;
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
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
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
  void replace(ASTNode const *Old, ASTNode *New) noexcept override;
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