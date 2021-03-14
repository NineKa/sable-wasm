#include "ASTNode.h"

#include "Module.h"

namespace mir::constant_expr {
Constant::Constant(std::int32_t Value)
    : ConstantExpr(ConstantExprKind::Constant), Storage(Value) {}
Constant::Constant(std::int64_t Value)
    : ConstantExpr(ConstantExprKind::Constant), Storage(Value) {}
Constant::Constant(float Value)
    : ConstantExpr(ConstantExprKind::Constant), Storage(Value) {}
Constant::Constant(double Value)
    : ConstantExpr(ConstantExprKind::Constant), Storage(Value) {}

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
  if (std::get<std::int32_t>(Storage)) return I32;
  if (std::get<std::int64_t>(Storage)) return I64;
  if (std::get<float>(Storage)) return F32;
  if (std::get<double>(Storage)) return F64;
  SABLE_UNREACHABLE();
}

void Constant::detach(ASTNode const *) noexcept { SABLE_UNREACHABLE(); }

bool Constant::classof(ConstantExpr const *ConstExpr) {
  return ConstExpr->getConstantExprKind() == ConstantExprKind::Constant;
}

bool Constant::classof(ASTNode const *Node) {
  if (ConstantExpr::classof(Node))
    return Constant::classof(dyn_cast<ConstantExpr>(Node));
  return false;
}

GlobalGet::GlobalGet(Global *GlobalValue_)
    : ConstantExpr(ConstantExprKind::GlobalGet), GlobalValue() {
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
  SABLE_UNREACHABLE();
}

bool GlobalGet::classof(ConstantExpr const *ConstExpr) {
  return ConstExpr->getConstantExprKind() == ConstantExprKind::GlobalGet;
}

bool GlobalGet::classof(ASTNode const *Node) {
  if (ConstantExpr::classof(Node))
    return GlobalGet::classof(dyn_cast<ConstantExpr>(Node));
  return false;
}
} // namespace mir::constant_expr