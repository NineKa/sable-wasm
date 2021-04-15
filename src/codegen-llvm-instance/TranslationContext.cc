#include "TranslationContext.h"

#include "LLVMCodegen.h"

#include <range/v3/view/transform.hpp>

namespace codegen::llvm_instance {
llvm::Value *TranslationContext::getLocalInitializer(mir::Local const &Local) {
  IRBuilder Builder(Target);
  if (Local.isParameter()) {
    auto const &ParentFunction = *Local.getParent();
    auto LocalBegin = ParentFunction.getLocals().begin();
    auto Index = std::distance(LocalBegin, Local.getIterator());
    return (Target.arg_begin() + 1) + Index;
  }
  using ValueKind = bytecode::ValueTypeKind;
  switch (Local.getType().getKind()) {
  case ValueKind::I32: return Builder.getInt32(0);
  case ValueKind::I64: return Builder.getInt64(0);
  case ValueKind::F32: return Builder.getFloat(0.0f);
  case ValueKind::F64: return Builder.getDouble(0.0);
  case ValueKind::V128: return Builder.getIntN(128, 0);
  default: utility::unreachable();
  }
}

TranslationContext::TranslationContext(
    EntityLayout &Laytout_, mir::Function const &Source_,
    llvm::Function &Target_)
    : Layout(Laytout_), Source(Source_), Target(Target_) {
  auto &MIREntryBB = Source.getEntryBasicBlock();
  mir::passes::SimpleFunctionPassDriver<mir::passes::DominatorPass>
      DominatorPassDirver;
  DominatorTree = DominatorPassDirver(Source).buildDomTree(MIREntryBB);
  mir::passes::TypeInferPass TypeInferPass;
  TypeInferPass.prepare(Source, DominatorTree);
  TypeInferPass.run();
  TypeInferPass.finalize();
  TypePassResult = TypeInferPass.getResult();

  auto *LocalSetupBB = llvm::BasicBlock::Create(
      /* Context */ Target.getContext(),
      /* Name    */ "locals",
      /* Parent  */ std::addressof(Target));
  IRBuilder Builder(*LocalSetupBB);
  for (auto const &Local : Source.getLocals().asView()) {
    auto *LLVMLocal = Builder.CreateAlloca(Layout.convertType(Local.getType()));
    LocalMap.emplace(std::addressof(Local), LLVMLocal);
    Builder.CreateStore(getLocalInitializer(Local), LLVMLocal);
  }

  for (auto const &BasicBlock : Source.getBasicBlocks().asView()) {
    auto *BB = llvm::BasicBlock::Create(
        /* Context */ Target.getContext(),
        /* Name    */ llvm::StringRef(BasicBlock.getName()),
        /* Parent  */ std::addressof(Target));
    auto Entry = std::make_pair(BB, BB);
    BasicBlockMap.emplace(std::addressof(BasicBlock), Entry);
  }

  auto [LLVMEntryBBFirst, LLVMEntryBBLast] = this->operator[](MIREntryBB);
  utility::ignore(LLVMEntryBBLast);
  Builder.CreateBr(LLVMEntryBBFirst);
}

TranslationContext::~TranslationContext() noexcept = default;

namespace {
#ifndef NDEBUG
bool isV128Value(llvm::Value const *Value) {
  if (Value->getType()->isIntegerTy(128)) return true;
  if (Value->getType()->isVectorTy()) {
    auto *CastedTy = llvm::dyn_cast<llvm::VectorType>(Value->getType());
    auto *ElementTy = CastedTy->getElementType();
    auto NumElement = CastedTy->getElementCount().getValue();
    if (ElementTy->isIntegerTy(8) && (NumElement == 16)) return true; // i8x16
    if (ElementTy->isIntegerTy(16) && (NumElement == 8)) return true; // i16x8
    if (ElementTy->isIntegerTy(32) && (NumElement == 4)) return true; // i32x4
    if (ElementTy->isIntegerTy(64) && (NumElement == 2)) return true; // i64x2
    if (ElementTy->isFloatTy() && (NumElement == 4)) return true;     // f32x4
    if (ElementTy->isDoubleTy() && (NumElement == 2)) return true;    // f64x2
  }
  return true;
}
#endif
} // namespace

llvm::Value *
TranslationContext::operator[](mir::Instruction const &Instruction) const {
  auto SearchIter = ValueMap.find(std::addressof(Instruction));
  assert(SearchIter != ValueMap.end());
#ifndef NDEBUG
  auto *Value = std::get<1>(*SearchIter);
  auto ExpectType = TypePassResult[Instruction];
  switch (ExpectType.getKind()) {
  case mir::TypeKind::Primitive: {
    using VK = bytecode::ValueTypeKind;
    switch (ExpectType.asPrimitive().getKind()) {
    case VK::I32: utility::expect(Value->getType()->isIntegerTy(32)); break;
    case VK::I64: utility::expect(Value->getType()->isIntegerTy(64)); break;
    case VK::F32: utility::expect(Value->getType()->isFloatTy()); break;
    case VK::F64: utility::expect(Value->getType()->isDoubleTy()); break;
    case VK::V128: utility::expect(isV128Value(Value)); break;
    default: utility::unreachable();
    }
    break;
  }
  case mir::TypeKind::Aggregate: {
    // clang-format off
    auto Members = ExpectType.asAggregate()
      | ranges::views::transform([&](bytecode::ValueType const &ValueType) {
          return Layout.convertType(ValueType);
        })
      | ranges::to<std::vector<llvm::Type *>>();
    // clang-format on
    auto *StructTy = llvm::StructType::get(Target.getContext(), Members);
    auto *StructPtrTy = llvm::PointerType::getUnqual(StructTy);
    utility::expect(StructPtrTy == Value->getType());
    break;
  }
  default: utility::unreachable();
  }
#endif
  return std::get<1>(*SearchIter);
}

llvm::Value *TranslationContext::operator[](mir::Local const &Local) const {
  auto SearchIter = LocalMap.find(std::addressof(Local));
  assert(SearchIter != LocalMap.end());
  return std::get<1>(*SearchIter);
}

std::pair<llvm::BasicBlock *, llvm::BasicBlock *>
TranslationContext::operator[](mir::BasicBlock const &BasicBlock) const {
  auto SearchIter = BasicBlockMap.find(std::addressof(BasicBlock));
  assert(SearchIter != BasicBlockMap.end());
  return std::get<1>(*SearchIter);
}

llvm::BasicBlock *
TranslationContext::createBasicBlock(mir::BasicBlock const &BasicBlock) {
  llvm::BasicBlock *InsertPos = nullptr;
  if (std::next(BasicBlock.getIterator()) != Source.getBasicBlocks().end()) {
    auto const &NextBB = *std::next(BasicBlock.getIterator());
    auto [LLVMNextBB, LLVMLastBB] = this->operator[](NextBB);
    utility::ignore(LLVMLastBB);
    InsertPos = LLVMNextBB;
  }
  auto [FirstBB, LastBB] = this->operator[](BasicBlock);
  LastBB = llvm::BasicBlock::Create(
      /* Context       */ Target.getContext(),
      /* Name          */ llvm::StringRef(BasicBlock.getName()),
      /* Parent        */ std::addressof(Target),
      /* Insert Before */ InsertPos);
  auto NewEntry = std::make_pair(FirstBB, LastBB);
  BasicBlockMap.emplace(std::addressof(BasicBlock), NewEntry);
  return LastBB;
}

void TranslationContext::setValueMapping(
    mir::Instruction const &Inst, llvm::Value *Value) {
  ValueMap.emplace(std::addressof(Inst), Value);
}

EntityLayout const &TranslationContext::getLayout() const { return Layout; }

std::shared_ptr<mir::passes::DominatorTreeNode>
TranslationContext::getDominatorTree() const {
  return DominatorTree;
}

mir::passes::TypeInferPassResult const &
TranslationContext::getInferredType() const {
  return TypePassResult;
}

mir::Function const &TranslationContext::getSource() const { return Source; }

llvm::Function &TranslationContext::getTarget() { return Target; }

llvm::Argument *TranslationContext::getInstancePtr() const {
  return Target.arg_begin();
}
} // namespace codegen::llvm_instance