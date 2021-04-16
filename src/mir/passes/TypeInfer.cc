#include "TypeInfer.h"
#include "../Binary.h"
#include "../Branch.h"
#include "../Compare.h"
#include "../Unary.h"
#include "../Vector.h"

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
class TypeInferVisitor :
    public mir::InstVisitorBase<TypeInferVisitor, Type>,
    public minsts::UnaryVisitorBase<TypeInferVisitor, Type>,
    public minsts::BinaryVisitorBase<TypeInferVisitor, Type>,
    public minsts::VectorExtractVisitorBase<TypeInferVisitor, Type> {
  TypeInferPassResult::TypeMap &Types;

  Type const &getType(mir::Instruction const *Instruction) const {
    auto SearchIter = Types.find(Instruction);
    assert(SearchIter != Types.end());
    return std::get<1>(*SearchIter);
  }

public:
  using InstVisitorBase::visit;

  TypeInferVisitor(TypeInferPassResult::TypeMap &Types_) : Types(Types_) {}

  Type operator()(minsts::Unreachable const *) { return Type::BuildUnit(); }
  Type operator()(minsts::Branch const *) { return Type::BuildUnit(); }
  Type operator()(minsts::Return const *) { return Type::BuildUnit(); }

  Type operator()(minsts::Call const *Inst) {
    auto const &TargetType = Inst->getTarget()->getType();
    if (TargetType.isVoidResult()) return Type::BuildUnit();
    if (TargetType.isSingleValueResult())
      return Type::BuildPrimitive(TargetType.getResultTypes()[0]);
    assert(TargetType.isMultiValueResult());
    return Type::BuildAggregate(TargetType.getResultTypes());
  }

  Type operator()(minsts::CallIndirect const *Inst) {
    if (Inst->getExpectType().isVoidResult()) return Type::BuildUnit();
    if (Inst->getExpectType().isSingleValueResult())
      return Type::BuildPrimitive(Inst->getExpectType().getResultTypes()[0]);
    assert(Inst->getExpectType().isMultiValueResult());
    return Type::BuildAggregate(Inst->getExpectType().getResultTypes());
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

  Type operator()(minsts::Compare const *) {
    using namespace bytecode::valuetypes;
    return Type::BuildPrimitive(I32);
  }

  Type operator()(minsts::unary::IntUnary const *Inst) {
    using namespace bytecode::valuetypes;
    auto const &OperandTy = getType(Inst->getOperand());
    if (!OperandTy.isIntegral()) return Type::BuildBottom();
    return OperandTy;
  }

  Type operator()(minsts::unary::FPUnary const *Inst) {
    auto const &OperandTy = getType(Inst->getOperand());
    if (!OperandTy.isFloatingPoint()) return Type::BuildBottom();
    return OperandTy;
  }

  Type operator()(minsts::Unary const *Inst) {
    return UnaryVisitorBase::visit(Inst);
  }

  Type operator()(minsts::binary::IntBinary const *Inst) {
    using namespace bytecode::valuetypes;
    auto const &LHSTy = getType(Inst->getLHS());
    auto const &RHSTy = getType(Inst->getRHS());
    if (LHSTy != RHSTy) return Type::BuildBottom();
    if (!LHSTy.isIntegral()) return Type::BuildBottom();
    return LHSTy;
  }

  Type operator()(minsts::binary::FPBinary const *Inst) {
    using namespace bytecode::valuetypes;
    auto const &LHSTy = getType(Inst->getLHS());
    auto const &RHSTy = getType(Inst->getRHS());
    if (LHSTy != RHSTy) return Type::BuildBottom();
    if (!LHSTy.isFloatingPoint()) return Type::BuildBottom();
    return LHSTy;
  }

  Type operator()(minsts::Binary const *Inst) {
    return BinaryVisitorBase::visit(Inst);
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
    std::vector<bytecode::ValueType> Members;
    Members.reserve(Inst->getNumArguments());
    for (auto const *Instruction : Inst->getArguments()) {
      if (!getType(Instruction).isPrimitive()) return Type::BuildBottom();
      Members.push_back(getType(Instruction).asPrimitive());
    }
    return Type::BuildAggregate(Members);
  }

  Type operator()(minsts::Unpack const *Inst) {
    auto const &OperandTy = getType(Inst->getOperand());
    if (!OperandTy.isAggregate()) return Type::BuildBottom();
    if (!(Inst->getIndex() < OperandTy.asAggregate().size()))
      return Type::BuildBottom();
    auto MemberTy = OperandTy.asAggregate()[Inst->getIndex()];
    return Type::BuildPrimitive(MemberTy);
  }

  Type operator()(minsts::Phi const *Inst) {
    return Type::BuildPrimitive(Inst->getType());
  }

  Type operator()(minsts::VectorSplat const *) {
    return Type::BuildPrimitive(bytecode::valuetypes::V128);
  }

  Type operator()(minsts::vector_extract::SIMD128IntExtract const *Inst) {
    if (!getType(Inst->getOperand()).isPrimitiveV128())
      return Type::BuildBottom();
    using ElementKind = mir::SIMD128IntElementKind;
    using namespace bytecode::valuetypes;
    switch (Inst->getLaneInfo().getElementKind()) {
    case ElementKind::I8:
    case ElementKind::I16:
    case ElementKind::I32: return Type::BuildPrimitive(I32);
    case ElementKind::I64: return Type::BuildPrimitive(I64);
    default: utility::unreachable();
    }
  }

  Type operator()(minsts::vector_extract::SIMD128FPExtract const *Inst) {
    if (!getType(Inst->getOperand()).isPrimitiveV128())
      return Type::BuildBottom();
    using ElementKind = mir::SIMD128FPElementKind;
    using namespace bytecode::valuetypes;
    switch (Inst->getLaneInfo().getElementKind()) {
    case ElementKind::F32: return Type::BuildPrimitive(F32);
    case ElementKind::F64: return Type::BuildPrimitive(F64);
    default: utility::unreachable();
    }
  }

  Type operator()(minsts::VectorExtract const *Inst) {
    return VectorExtractVisitorBase::visit(Inst);
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