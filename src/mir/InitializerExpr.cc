#include "InitializerExpr.h"

#include "Module.h"

namespace mir {
InitializerExpr::InitializerExpr(InitializerExprKind Kind_)
    : ASTNode(ASTNodeKind::InitializerExpr), Kind(Kind_) {}
// clang-format off
InitializerExprKind InitializerExpr::getInitializerExprKind() const 
{ return Kind; }
bool InitializerExpr::classof(ASTNode const *Node) 
{ return Node->getASTNodeKind() == ASTNodeKind::InitializerExpr; }
// clang-format on
} // namespace mir

namespace mir::initializer {
Constant::Constant(std::int32_t Value)
    : InitializerExpr(InitializerExprKind::Constant), Storage(Value) {}
Constant::Constant(std::int64_t Value)
    : InitializerExpr(InitializerExprKind::Constant), Storage(Value) {}
Constant::Constant(float Value)
    : InitializerExpr(InitializerExprKind::Constant), Storage(Value) {}
Constant::Constant(double Value)
    : InitializerExpr(InitializerExprKind::Constant), Storage(Value) {}

std::int32_t &Constant::asI32() { return std::get<std::int32_t>(Storage); }
std::int64_t &Constant::asI64() { return std::get<std::int64_t>(Storage); }
float &Constant::asF32() { return std::get<float>(Storage); }
double &Constant::asF64() { return std::get<double>(Storage); }
std::int32_t Constant::asI32() const { return std::get<std::int32_t>(Storage); }
std::int64_t Constant::asI64() const { return std::get<std::int64_t>(Storage); }
float Constant::asF32() const { return std::get<float>(Storage); }
double Constant::asF64() const { return std::get<double>(Storage); }

bytecode::ValueType Constant::getValueType() const {
  using namespace bytecode::valuetypes;
  if (std::holds_alternative<std::int32_t>(Storage)) return I32;
  if (std::holds_alternative<std::int64_t>(Storage)) return I64;
  if (std::holds_alternative<float>(Storage)) return F32;
  if (std::holds_alternative<double>(Storage)) return F64;
  utility::unreachable();
}

void Constant::detach(ASTNode const *) noexcept { utility::unreachable(); }

bool Constant::classof(InitializerExpr const *ConstExpr) {
  return ConstExpr->getInitializerExprKind() == InitializerExprKind::Constant;
}

bool Constant::classof(ASTNode const *Node) {
  if (InitializerExpr::classof(Node))
    return Constant::classof(dyn_cast<InitializerExpr>(Node));
  return false;
}

GlobalGet::GlobalGet(Global *GlobalValue_)
    : InitializerExpr(InitializerExprKind::GlobalGet), GlobalValue() {
  setGlobalValue(GlobalValue_);
}

GlobalGet::~GlobalGet() noexcept {
  if (GlobalValue != nullptr) GlobalValue->remove_use(this);
}

Global *GlobalGet::getGlobalValue() const { return GlobalValue; }

void GlobalGet::setGlobalValue(Global *GlobalValue_) {
  if (GlobalValue != nullptr) GlobalValue->remove_use(this);
  if (GlobalValue_ != nullptr) GlobalValue_->add_use(this);
  GlobalValue = GlobalValue_;
}

void GlobalGet::detach(ASTNode const *Node) noexcept {
  if (GlobalValue == Node) {
    GlobalValue = nullptr;
    return;
  }
  utility::unreachable();
}

bool GlobalGet::classof(InitializerExpr const *ConstExpr) {
  return ConstExpr->getInitializerExprKind() == InitializerExprKind::GlobalGet;
}

bool GlobalGet::classof(ASTNode const *Node) {
  if (InitializerExpr::classof(Node))
    return GlobalGet::classof(dyn_cast<InitializerExpr>(Node));
  return false;
}
} // namespace mir::initializer