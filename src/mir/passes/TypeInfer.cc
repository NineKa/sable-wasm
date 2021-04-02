#include "TypeInfer.h"

#include <range/v3/view/transform.hpp>

namespace mir::passes {

TypeInferPassResult::TypeInferPassResult(std::shared_ptr<TypeMap> Types_)
    : Types(std::move(Types_)) {}

mir::Type const &
TypeInferPassResult::operator[](mir::Instruction const &Instruction) const {
  auto SearchIter = Types->find(std::addressof(Instruction));
  assert(SearchIter != Types->end());
  return std::get<1>(*SearchIter);
}

void TypeInferPass::prepare(mir::Function const &Function_) {
  using DirverType =
      mir::passes::SimpleFunctionPassDriver<mir::passes::DominatorPass>;
  auto const &EntryBB = Function_.getEntryBasicBlock();
  auto DomTree_ = DirverType()(Function_).buildDomTree(EntryBB);
  prepare(Function_, std::move(DomTree_));
}

void TypeInferPass::prepare(
    mir::Function const &Function_,
    std::shared_ptr<DominatorTreeNode> DomTree_) {
  Types = std::make_shared<TypeInferPassResult::TypeMap>();
  Function = std::addressof(Function_);
  DomTree = std::move(DomTree_);
}

namespace {
namespace minsts = mir::instructions;
class TypeInferVisitor : public mir::InstVisitorBase<TypeInferVisitor, Type> {
  TypeInferPassResult::TypeMap &Types;

  Type const &getType(mir::Instruction const *Instruction) const {
    auto SearchIter = Types.find(Instruction);
    assert(SearchIter != Types.end());
    return std::get<1>(*SearchIter);
  }

public:
  TypeInferVisitor(TypeInferPassResult::TypeMap &Types_) : Types(Types_) {}

  Type operator()(minsts::Unreachable const *) { return Type::BuildUnit(); }
  Type operator()(minsts::Branch const *) { return Type::BuildUnit(); }
  Type operator()(minsts::BranchTable const *) { return Type::BuildUnit(); }
  Type operator()(minsts::Return const *) { return Type::BuildUnit(); }

  Type operator()(minsts::Call const *Inst) {
    auto const &TargetType = Inst->getTarget()->getType();
    if (TargetType.isVoidResult()) return Type::BuildUnit();
    if (TargetType.isSingleValueResult())
      return Type::BuildPrimitive(TargetType.getResultTypes()[0]);
    assert(TargetType.isMultiValueResult());
    // clang-format off
    auto MemberTypes = TargetType.getResultTypes()
      | ranges::views::transform([](bytecode::ValueType const &ValueType) {
          return Type::BuildPrimitive(ValueType);
        })
      | ranges::to<std::vector<Type>>();
    // clang-format on
    return Type::BuildAggregate(MemberTypes);
  }

  Type operator()(minsts::CallIndirect const *Inst) {
    if (Inst->getExpectType().isVoidResult()) return Type::BuildUnit();
    if (Inst->getExpectType().isSingleValueResult())
      return Type::BuildPrimitive(Inst->getExpectType().getResultTypes()[0]);
    assert(Inst->getExpectType().isMultiValueResult());
    // clang-format off
    auto MemberTypes = Inst->getExpectType().getResultTypes()
      | ranges::views::transform([](bytecode::ValueType const &ValueType) {
          return Type::BuildPrimitive(ValueType);
        })
      | ranges::to<std::vector<Type>>();
    // clang-format on
    return Type::BuildAggregate(MemberTypes);
  }

  Type operator()(minsts::Select const *Inst) {
    auto const &TrueTy = getType(Inst->getTrue());
    auto const &FalseTy = getType(Inst->getFalse());
    if (TrueTy != FalseTy) return Type::BuildBottom();
    return TrueTy;
  }

  Type operator()(minsts::LocalGet const *Inst) {
    return Type::BuildPrimitive(Inst->getTarget()->getType());
  }

  Type operator()(minsts::LocalSet const *) { return Type::BuildUnit(); }

  Type operator()(minsts::GlobalGet const *Inst) {
    auto const &GlobalType = Inst->getTarget()->getType();
    return Type::BuildPrimitive(GlobalType.getType());
  }

  Type operator()(minsts::GlobalSet const *) { return Type::BuildUnit(); }

  Type operator()(minsts::Constant const *Inst) {
    return Type::BuildPrimitive(Inst->getValueType());
  }

  Type operator()(minsts::IntUnaryOp const *Inst) {
    using namespace bytecode::valuetypes;
    auto const &OperandTy = getType(Inst->getOperand());
    if (!OperandTy.isIntegral()) return Type::BuildBottom();
    switch (Inst->getOperator()) {
    case minsts::IntUnaryOperator::Eqz: return Type::BuildPrimitive(I32);
    case minsts::IntUnaryOperator::Clz:
    case minsts::IntUnaryOperator::Ctz:
    case minsts::IntUnaryOperator::Popcnt: return OperandTy;
    default: utility::unreachable();
    }
  }

  Type operator()(minsts::IntBinaryOp const *Inst) {
    using namespace bytecode::valuetypes;
    auto const &LHSTy = getType(Inst->getLHS());
    auto const &RHSTy = getType(Inst->getRHS());
    if (LHSTy != RHSTy) return Type::BuildBottom();
    if (!LHSTy.isIntegral()) return Type::BuildBottom();
    switch (Inst->getOperator()) {
    case minsts::IntBinaryOperator::Eq:
    case minsts::IntBinaryOperator::Ne:
    case minsts::IntBinaryOperator::LtS:
    case minsts::IntBinaryOperator::LtU:
    case minsts::IntBinaryOperator::GtS:
    case minsts::IntBinaryOperator::GtU:
    case minsts::IntBinaryOperator::LeS:
    case minsts::IntBinaryOperator::LeU:
    case minsts::IntBinaryOperator::GeS:
    case minsts::IntBinaryOperator::GeU: return Type::BuildPrimitive(I32);
    case minsts::IntBinaryOperator::Add:
    case minsts::IntBinaryOperator::Sub:
    case minsts::IntBinaryOperator::Mul:
    case minsts::IntBinaryOperator::DivS:
    case minsts::IntBinaryOperator::DivU:
    case minsts::IntBinaryOperator::RemS:
    case minsts::IntBinaryOperator::RemU:
    case minsts::IntBinaryOperator::And:
    case minsts::IntBinaryOperator::Or:
    case minsts::IntBinaryOperator::Xor:
    case minsts::IntBinaryOperator::Shl:
    case minsts::IntBinaryOperator::ShrS:
    case minsts::IntBinaryOperator::ShrU:
    case minsts::IntBinaryOperator::Rotl:
    case minsts::IntBinaryOperator::Rotr: return LHSTy;
    default: utility::unreachable();
    }
  }

  Type operator()(minsts::FPUnaryOp const *Inst) {
    auto const &OperandTy = getType(Inst->getOperand());
    if (!OperandTy.isFloatingPoint()) return Type::BuildBottom();
    switch (Inst->getOperator()) {
    case minsts::FPUnaryOperator::Abs:
    case minsts::FPUnaryOperator::Neg:
    case minsts::FPUnaryOperator::Ceil:
    case minsts::FPUnaryOperator::Floor:
    case minsts::FPUnaryOperator::Trunc:
    case minsts::FPUnaryOperator::Nearest:
    case minsts::FPUnaryOperator::Sqrt: return OperandTy;
    default: utility::unreachable();
    }
  }

  Type operator()(minsts::FPBinaryOp const *Inst) {
    using namespace bytecode::valuetypes;
    auto const &LHSTy = getType(Inst->getLHS());
    auto const &RHSTy = getType(Inst->getRHS());
    if (LHSTy != RHSTy) return Type::BuildBottom();
    if (!LHSTy.isFloatingPoint()) return Type::BuildBottom();
    switch (Inst->getOperator()) {
    case minsts::FPBinaryOperator::Eq:
    case minsts::FPBinaryOperator::Ne:
    case minsts::FPBinaryOperator::Lt:
    case minsts::FPBinaryOperator::Gt:
    case minsts::FPBinaryOperator::Le:
    case minsts::FPBinaryOperator::Ge: return Type::BuildPrimitive(I32);
    case minsts::FPBinaryOperator::Add:
    case minsts::FPBinaryOperator::Sub:
    case minsts::FPBinaryOperator::Mul:
    case minsts::FPBinaryOperator::Div:
    case minsts::FPBinaryOperator::Min:
    case minsts::FPBinaryOperator::Max:
    case minsts::FPBinaryOperator::CopySign: return LHSTy;
    default: utility::unreachable();
    }
  }

  Type operator()(minsts::Load const *Inst) {
    return Type::BuildPrimitive(Inst->getType());
  }

  Type operator()(minsts::Store const *) { return Type::BuildUnit(); }
  Type operator()(minsts::MemoryGuard const *) { return Type::BuildUnit(); }

  Type operator()(minsts::MemoryGrow const *) {
    using namespace bytecode::valuetypes;
    return Type::BuildPrimitive(I32);
  }

  Type operator()(minsts::MemorySize const *) {
    using namespace bytecode::valuetypes;
    return Type::BuildPrimitive(I32);
  }

  Type operator()(minsts::Cast const *Inst) {
    return Type::BuildPrimitive(Inst->getType());
  }

  Type operator()(minsts::Extend const *Inst) {
    auto const &OperandTy = getType(Inst->getOperand());
    if (!OperandTy.isIntegral()) return Type::BuildBottom();
    return OperandTy;
  }

  Type operator()(minsts::Pack const *Inst) {
    // clang-format off
    auto Members = Inst->getArguments()
      | ranges::views::transform([&](mir::Instruction const *Instruction) {
          return getType(Instruction);
        })
      | ranges::to<std::vector<Type>>();
    // clang-format on
    return Type::BuildAggregate(Members);
  }

  Type operator()(minsts::Unpack const *Inst) {
    auto const &OperandTy = getType(Inst->getOperand());
    if (!OperandTy.isAggregate()) return Type::BuildBottom();
    if (!(Inst->getIndex() < OperandTy.asAggregate().size()))
      return Type::BuildBottom();
    return OperandTy.asAggregate()[Inst->getIndex()];
  }

  Type operator()(minsts::Phi const *Inst) {
    return Type::BuildPrimitive(Inst->getType());
  }
};
} // namespace

PassStatus TypeInferPass::run() {
  TypeInferVisitor Visitor(*Types);
  for (auto const *BasicBlockPtr : DomTree->asPreorder())
    for (auto const &Instruction : *BasicBlockPtr) {
      auto const *InstPtr = std::addressof(Instruction);
      Types->emplace(InstPtr, Visitor.visit(InstPtr));
    }
  return PassStatus::Converged;
}

void TypeInferPass::finalize() {
  Function = nullptr;
  DomTree.reset();
}

TypeInferPassResult TypeInferPass::getResult() const {
  return TypeInferPassResult(Types);
}

bool TypeInferPass::isSkipped(mir::Function const &Function_) const {
  return Function_.isDeclaration();
}

} // namespace mir::passes