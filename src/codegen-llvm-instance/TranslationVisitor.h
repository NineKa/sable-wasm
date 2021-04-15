#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_TRANSLATION_VISITOR
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_TRANSLATION_VISITOR

#include "../mir/Binary.h"
#include "../mir/Branch.h"
#include "../mir/Compare.h"
#include "../mir/Instruction.h"
#include "../mir/Module.h"
#include "../mir/Unary.h"

#include <llvm/IR/Value.h>

namespace codegen::llvm_instance {
class IRBuilder;
class TranslationContext;
// clang-format off
class TranslationVisitor :
    public mir::InstVisitorBase<TranslationVisitor, llvm::Value *>,
    public mir::instructions::BranchVisitorBase<TranslationVisitor, llvm::Value *>,
    public mir::instructions::CompareVisitorBase<TranslationVisitor, llvm::Value *>,
    public mir::instructions::UnaryVisitorBase<TranslationVisitor, llvm::Value *>,
    public mir::instructions::BinaryVisitorBase<TranslationVisitor, llvm::Value *>
// clang-format on
{
  TranslationContext &Context;
  IRBuilder &Builder;

  llvm::Value *getMemoryRWPtr(mir::Memory const &Memory, llvm::Value *Address);

public:
  TranslationVisitor(TranslationContext &Context_, IRBuilder &Builder_);
  TranslationVisitor(TranslationVisitor const &) = delete;
  TranslationVisitor(TranslationVisitor &&) noexcept = delete;
  TranslationVisitor &operator=(TranslationVisitor const &) = delete;
  TranslationVisitor &operator=(TranslationVisitor &&) noexcept = delete;
  ~TranslationVisitor() noexcept;

#define SABLE_ON(InstName)                                                     \
  llvm::Value *operator()(mir::instructions::InstName const *);

  SABLE_ON(Unreachable)

  SABLE_ON(branch::Unconditional)
  SABLE_ON(branch::Conditional)
  SABLE_ON(branch::Switch)
  SABLE_ON(Branch)

  SABLE_ON(Return)
  SABLE_ON(Call)
  SABLE_ON(CallIndirect)
  SABLE_ON(Select)
  SABLE_ON(LocalGet)
  SABLE_ON(LocalSet)
  SABLE_ON(GlobalGet)
  SABLE_ON(GlobalSet)
  SABLE_ON(Constant)

  SABLE_ON(compare::IntCompare)
  SABLE_ON(compare::FPCompare)
  SABLE_ON(compare::SIMD128IntCompare)
  SABLE_ON(compare::SIMD128FPCompare)
  SABLE_ON(Compare)

  SABLE_ON(unary::IntUnary)
  SABLE_ON(unary::FPUnary)
  SABLE_ON(unary::SIMD128Unary)
  SABLE_ON(unary::SIMD128IntUnary)
  SABLE_ON(unary::SIMD128FPUnary)
  SABLE_ON(Unary)

  SABLE_ON(binary::IntBinary)
  SABLE_ON(binary::FPBinary)
  SABLE_ON(binary::SIMD128Binary)
  SABLE_ON(binary::SIMD128IntBinary)
  SABLE_ON(binary::SIMD128FPBinary)
  SABLE_ON(Binary)

  SABLE_ON(Load)
  SABLE_ON(Store)
  SABLE_ON(MemoryGuard)
  SABLE_ON(MemoryGrow)
  SABLE_ON(MemorySize)
  SABLE_ON(Cast)
  SABLE_ON(Extend)
  SABLE_ON(Pack)
  SABLE_ON(Unpack)
  SABLE_ON(Phi)
#undef SABLE_ON

  llvm::Value *visit(mir::Instruction const *Instruction);
};

} // namespace codegen::llvm_instance

#endif