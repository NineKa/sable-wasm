#include "ExprBuilderDelegate.h"

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/core.hpp>
#include <range/v3/iterator.hpp>

namespace parser {
using namespace bytecode::instructions;

template <bytecode::instruction T>
T *ExprBuilderDelegate::getCurrentScopeEnclosingInst() {
  assert(!Scopes.empty() && !Scopes.top().empty());
  bytecode::Instruction *Instruction = Scopes.top().back();
  assert(bytecode::is_a<T>(Instruction));
  return static_cast<T *>(Instruction);
}

template <bytecode::instruction T, typename... ArgTypes>
void ExprBuilderDelegate::addInst(ArgTypes &&...Args) {
  CExpr.push_back(
      bytecode::InstructionPtr::Build<T>(std::forward<ArgTypes>(Args)...));
}

#define EVENT(Name) void ExprBuilderDelegate::Name

EVENT(enterExpression)() { CExpr.clear(); }
EVENT(exitExpression)() {}

EVENT(onInstUnreachable)() { addInst<Unreachable>(); }
EVENT(onInstNop)() { addInst<Nop>(); }
EVENT(enterInstBlock)(bytecode::BlockResultType Type) {
  addInst<Block>(Type, bytecode::Expression{});
  Scopes.push(std::move(CExpr));
  CExpr.clear();
}
EVENT(exitInstBlock)() {
  auto *EnclosingBlock = getCurrentScopeEnclosingInst<Block>();
  EnclosingBlock->Body = std::move(CExpr);
  CExpr = std::move(Scopes.top());
  Scopes.pop();
}
EVENT(enterInstLoop)(bytecode::BlockResultType Type) {
  addInst<Loop>(Type, bytecode::Expression{});
  Scopes.push(std::move(CExpr));
  CExpr.clear();
}
EVENT(exitInstLoop)() {
  auto *EnclosingLoop = getCurrentScopeEnclosingInst<Loop>();
  EnclosingLoop->Body = std::move(CExpr);
  CExpr = std::move(Scopes.top());
  Scopes.pop();
}
EVENT(enterInstIf)(bytecode::BlockResultType Type) {
  addInst<If>(Type, bytecode::Expression{}, std::nullopt);
  Scopes.push(std::move(CExpr));
  CExpr.clear();
}
EVENT(enterInstElse)() {
  auto *EnclosingIf = getCurrentScopeEnclosingInst<If>();
  EnclosingIf->True = std::move(CExpr);
  EnclosingIf->False = bytecode::Expression{};
  CExpr.clear();
}
EVENT(exitInstIf)() {
  auto *EnclosingIf = getCurrentScopeEnclosingInst<If>();
  if (EnclosingIf->False.has_value()) {
    EnclosingIf->False = std::move(CExpr);
  } else {
    EnclosingIf->True = std::move(CExpr);
  }
  CExpr = std::move(Scopes.top());
  Scopes.pop();
}
EVENT(onInstBr)(bytecode::LabelIDX Index) { addInst<Br>(Index); }
EVENT(onInstBrIf)(bytecode::LabelIDX Index) { addInst<BrIf>(Index); }
EVENT(onInstBrTable)
(bytecode::LabelIDX DefaultTarget,
 std::span<bytecode::LabelIDX const> Targets) {
  std::vector<bytecode::LabelIDX> Targets_;
  Targets_.reserve(Targets.size());
  ranges::copy(Targets, ranges::back_inserter(Targets_));
  addInst<BrTable>(std::move(Targets_), DefaultTarget);
}
EVENT(onInstReturn)() { addInst<Return>(); }
EVENT(onInstCall)(bytecode::FuncIDX IDX) { addInst<Call>(IDX); }
EVENT(onInstCallIndirect)(bytecode::TypeIDX IDX) { addInst<CallIndirect>(IDX); }

EVENT(onInstDrop)() { addInst<Drop>(); }
EVENT(onInstSelect)() { addInst<Select>(); }

#define LOCAL_EVENT(Name, InstName)                                            \
  EVENT(Name)(bytecode::LocalIDX IDX) { addInst<InstName>(IDX); }
LOCAL_EVENT(onInstLocalGet, LocalGet)
LOCAL_EVENT(onInstLocalSet, LocalSet)
LOCAL_EVENT(onInstLocalTee, LocalTee)
#undef LOCAL_EVENT

#define GLOBAL_EVENT(Name, InstName)                                           \
  EVENT(Name)(bytecode::GlobalIDX IDX) { addInst<InstName>(IDX); }
GLOBAL_EVENT(onInstGlobalGet, GlobalGet)
GLOBAL_EVENT(onInstGlobalSet, GlobalSet)
#undef GLOBAL_EVENT

#define MEMORY_EVENT(Name, InstName)                                           \
  EVENT(Name)(std::uint32_t Align, std::uint32_t Offset) {                     \
    addInst<InstName>(Align, Offset);                                          \
  }
// clang-format off
MEMORY_EVENT(onInstI32Load   , I32Load   )
MEMORY_EVENT(onInstI64Load   , I64Load   )
MEMORY_EVENT(onInstF32Load   , F32Load   )
MEMORY_EVENT(onInstF64Load   , F64Load   )
MEMORY_EVENT(onInstI32Load8S , I32Load8S )
MEMORY_EVENT(onInstI32Load8U , I32Load8U )
MEMORY_EVENT(onInstI32Load16S, I32Load16S)
MEMORY_EVENT(onInstI32Load16U, I32Load16U)
MEMORY_EVENT(onInstI64Load8S , I64Load8S )
MEMORY_EVENT(onInstI64Load8U , I64Load8U )
MEMORY_EVENT(onInstI64Load16S, I64Load16S)
MEMORY_EVENT(onInstI64Load16U, I64Load16U)
MEMORY_EVENT(onInstI64Load32S, I64Load32S)
MEMORY_EVENT(onInstI64Load32U, I64Load32U)
MEMORY_EVENT(onInstI32Store  , I32Store  )
MEMORY_EVENT(onInstI64Store  , I64Store  )
MEMORY_EVENT(onInstF32Store  , F32Store  )
MEMORY_EVENT(onInstF64Store  , F64Store  )
MEMORY_EVENT(onInstI32Store8 , I32Store8 )
MEMORY_EVENT(onInstI32Store16, I32Store16)
MEMORY_EVENT(onInstI64Store8 , I64Store8 )
MEMORY_EVENT(onInstI64Store16, I64Store16)
MEMORY_EVENT(onInstI64Store32, I64Store32)
// clang-format on
#undef MEMORY_EVENT
EVENT(onInstMemorySize)() { addInst<MemorySize>(); }
EVENT(onInstMemoryGrow)() { addInst<MemoryGrow>(); }

EVENT(onInstI32Const)(std::int32_t N) { addInst<I32Const>(N); }
EVENT(onInstI64Const)(std::int64_t N) { addInst<I64Const>(N); }
EVENT(onInstF32Const)(float N) { addInst<F32Const>(N); }
EVENT(onInstF64Const)(double N) { addInst<F64Const>(N); }

// clang-format off
EVENT(onInstI32Eqz           )() { addInst<I32Eqz           >(); }
EVENT(onInstI32Eq            )() { addInst<I32Eq            >(); }
EVENT(onInstI32Ne            )() { addInst<I32Ne            >(); }
EVENT(onInstI32LtS           )() { addInst<I32LtS           >(); }
EVENT(onInstI32LtU           )() { addInst<I32LtU           >(); }
EVENT(onInstI32GtS           )() { addInst<I32GtS           >(); }
EVENT(onInstI32GtU           )() { addInst<I32GtU           >(); }
EVENT(onInstI32LeS           )() { addInst<I32LeS           >(); }
EVENT(onInstI32LeU           )() { addInst<I32LeU           >(); }
EVENT(onInstI32GeS           )() { addInst<I32GeS           >(); }
EVENT(onInstI32GeU           )() { addInst<I32GeU           >(); }

EVENT(onInstI64Eqz           )() { addInst<I64Eqz           >(); }
EVENT(onInstI64Eq            )() { addInst<I64Eq            >(); }
EVENT(onInstI64Ne            )() { addInst<I64Ne            >(); }
EVENT(onInstI64LtS           )() { addInst<I64LtS           >(); }
EVENT(onInstI64LtU           )() { addInst<I64LtU           >(); }
EVENT(onInstI64GtS           )() { addInst<I64GtS           >(); }
EVENT(onInstI64GtU           )() { addInst<I64GtU           >(); }
EVENT(onInstI64LeS           )() { addInst<I64LeS           >(); }
EVENT(onInstI64LeU           )() { addInst<I64LeU           >(); }
EVENT(onInstI64GeS           )() { addInst<I64GeS           >(); }
EVENT(onInstI64GeU           )() { addInst<I64GeU           >(); }

EVENT(onInstF32Eq            )() { addInst<F32Eq            >(); }
EVENT(onInstF32Ne            )() { addInst<F32Ne            >(); }
EVENT(onInstF32Lt            )() { addInst<F32Lt            >(); }
EVENT(onInstF32Gt            )() { addInst<F32Gt            >(); }
EVENT(onInstF32Le            )() { addInst<F32Le            >(); }
EVENT(onInstF32Ge            )() { addInst<F32Ge            >(); }

EVENT(onInstF64Eq            )() { addInst<F64Eq            >(); }
EVENT(onInstF64Ne            )() { addInst<F64Ne            >(); }
EVENT(onInstF64Lt            )() { addInst<F64Lt            >(); }
EVENT(onInstF64Gt            )() { addInst<F64Gt            >(); }
EVENT(onInstF64Le            )() { addInst<F64Le            >(); }
EVENT(onInstF64Ge            )() { addInst<F64Ge            >(); }

EVENT(onInstI32Clz           )() { addInst<I32Clz           >(); }
EVENT(onInstI32Ctz           )() { addInst<I32Ctz           >(); }
EVENT(onInstI32Popcnt        )() { addInst<I32Popcnt        >(); }
EVENT(onInstI32Add           )() { addInst<I32Add           >(); }
EVENT(onInstI32Sub           )() { addInst<I32Sub           >(); }
EVENT(onInstI32Mul           )() { addInst<I32Mul           >(); }
EVENT(onInstI32DivS          )() { addInst<I32DivS          >(); }
EVENT(onInstI32DivU          )() { addInst<I32DivU          >(); }
EVENT(onInstI32RemS          )() { addInst<I32RemS          >(); }
EVENT(onInstI32RemU          )() { addInst<I32RemU          >(); }
EVENT(onInstI32And           )() { addInst<I32And           >(); }
EVENT(onInstI32Or            )() { addInst<I32Or            >(); }
EVENT(onInstI32Xor           )() { addInst<I32Xor           >(); }
EVENT(onInstI32Shl           )() { addInst<I32Shl           >(); }
EVENT(onInstI32ShrS          )() { addInst<I32ShrS          >(); }
EVENT(onInstI32ShrU          )() { addInst<I32ShrU          >(); }
EVENT(onInstI32Rotl          )() { addInst<I32Rotl          >(); }
EVENT(onInstI32Rotr          )() { addInst<I32Rotr          >(); }

EVENT(onInstI64Clz           )() { addInst<I64Clz           >(); }
EVENT(onInstI64Ctz           )() { addInst<I64Ctz           >(); }
EVENT(onInstI64Popcnt        )() { addInst<I64Popcnt        >(); }
EVENT(onInstI64Add           )() { addInst<I64Add           >(); }
EVENT(onInstI64Sub           )() { addInst<I64Sub           >(); }
EVENT(onInstI64Mul           )() { addInst<I64Mul           >(); }
EVENT(onInstI64DivS          )() { addInst<I64DivS          >(); }
EVENT(onInstI64DivU          )() { addInst<I64DivU          >(); }
EVENT(onInstI64RemS          )() { addInst<I64RemS          >(); }
EVENT(onInstI64RemU          )() { addInst<I64RemU          >(); }
EVENT(onInstI64And           )() { addInst<I64And           >(); }
EVENT(onInstI64Or            )() { addInst<I64Or            >(); }
EVENT(onInstI64Xor           )() { addInst<I64Xor           >(); }
EVENT(onInstI64Shl           )() { addInst<I64Shl           >(); }
EVENT(onInstI64ShrS          )() { addInst<I64ShrS          >(); }
EVENT(onInstI64ShrU          )() { addInst<I64ShrU          >(); }
EVENT(onInstI64Rotl          )() { addInst<I64Rotl          >(); }
EVENT(onInstI64Rotr          )() { addInst<I64Rotr          >(); }

EVENT(onInstF32Abs           )() { addInst<F32Abs           >(); }
EVENT(onInstF32Neg           )() { addInst<F32Neg           >(); }
EVENT(onInstF32Ceil          )() { addInst<F32Ceil          >(); }
EVENT(onInstF32Floor         )() { addInst<F32Floor         >(); }
EVENT(onInstF32Trunc         )() { addInst<F32Trunc         >(); }
EVENT(onInstF32Nearest       )() { addInst<F32Nearest       >(); }
EVENT(onInstF32Sqrt          )() { addInst<F32Sqrt          >(); }
EVENT(onInstF32Add           )() { addInst<F32Add           >(); }
EVENT(onInstF32Sub           )() { addInst<F32Sub           >(); }
EVENT(onInstF32Mul           )() { addInst<F32Mul           >(); }
EVENT(onInstF32Div           )() { addInst<F32Div           >(); }
EVENT(onInstF32Min           )() { addInst<F32Min           >(); }
EVENT(onInstF32Max           )() { addInst<F32Max           >(); }
EVENT(onInstF32CopySign      )() { addInst<F32CopySign      >(); }

EVENT(onInstF64Abs           )() { addInst<F64Abs           >(); }
EVENT(onInstF64Neg           )() { addInst<F64Neg           >(); }
EVENT(onInstF64Ceil          )() { addInst<F64Ceil          >(); }
EVENT(onInstF64Floor         )() { addInst<F64Floor         >(); }
EVENT(onInstF64Trunc         )() { addInst<F64Trunc         >(); }
EVENT(onInstF64Nearest       )() { addInst<F64Nearest       >(); }
EVENT(onInstF64Sqrt          )() { addInst<F64Sqrt          >(); }
EVENT(onInstF64Add           )() { addInst<F64Add           >(); }
EVENT(onInstF64Sub           )() { addInst<F64Sub           >(); }
EVENT(onInstF64Mul           )() { addInst<F64Mul           >(); }
EVENT(onInstF64Div           )() { addInst<F64Div           >(); }
EVENT(onInstF64Min           )() { addInst<F64Min           >(); }
EVENT(onInstF64Max           )() { addInst<F64Max           >(); }
EVENT(onInstF64CopySign      )() { addInst<F64CopySign      >(); }

EVENT(onInstI32WrapI64       )() { addInst<I32WrapI64       >(); }
EVENT(onInstI32TruncF32S     )() { addInst<I32TruncF32S     >(); }
EVENT(onInstI32TruncF32U     )() { addInst<I32TruncF32U     >(); }
EVENT(onInstI32TruncF64S     )() { addInst<I32TruncF64S     >(); }
EVENT(onInstI32TruncF64U     )() { addInst<I32TruncF64U     >(); }
EVENT(onInstI64ExtendI32S    )() { addInst<I64ExtendI32S    >(); }
EVENT(onInstI64ExtendI32U    )() { addInst<I64ExtendI32U    >(); }
EVENT(onInstI64TruncF32S     )() { addInst<I64TruncF32S     >(); }
EVENT(onInstI64TruncF32U     )() { addInst<I64TruncF32U     >(); }
EVENT(onInstI64TruncF64S     )() { addInst<I64TruncF64S     >(); }
EVENT(onInstI64TruncF64U     )() { addInst<I64TruncF64U     >(); }
EVENT(onInstF32ConvertI32S   )() { addInst<F32ConvertI32S   >(); }
EVENT(onInstF32ConvertI32U   )() { addInst<F32ConvertI32U   >(); }
EVENT(onInstF32ConvertI64S   )() { addInst<F32ConvertI64S   >(); }
EVENT(onInstF32ConvertI64U   )() { addInst<F32ConvertI64U   >(); }
EVENT(onInstF32DemoteF64     )() { addInst<F32DemoteF64     >(); }
EVENT(onInstF64ConvertI32S   )() { addInst<F64ConvertI32S   >(); }
EVENT(onInstF64ConvertI32U   )() { addInst<F64ConvertI32U   >(); }
EVENT(onInstF64ConvertI64S   )() { addInst<F64ConvertI64S   >(); }
EVENT(onInstF64ConvertI64U   )() { addInst<F64ConvertI64U   >(); }
EVENT(onInstF64PromoteF32    )() { addInst<F64PromoteF32    >(); }
EVENT(onInstI32ReinterpretF32)() { addInst<I32ReinterpretF32>(); }
EVENT(onInstI64ReinterpretF64)() { addInst<I64ReinterpretF64>(); }
EVENT(onInstF32ReinterpretI32)() { addInst<F32ReinterpretI32>(); }
EVENT(onInstF64ReinterpretI64)() { addInst<F64ReinterpretI64>(); }

EVENT(onInstI32Extend8S      )() { addInst<I32Extend8S      >(); }
EVENT(onInstI32Extend16S     )() { addInst<I32Extend16S     >(); }
EVENT(onInstI64Extend8S      )() { addInst<I64Extend8S      >(); }
EVENT(onInstI64Extend16S     )() { addInst<I64Extend16S     >(); }
EVENT(onInstI64Extend32S     )() { addInst<I64Extend32S     >(); }

EVENT(onInstI32TruncSatF32S  )() { addInst<I32TruncSatF32S  >(); }
EVENT(onInstI32TruncSatF32U  )() { addInst<I32TruncSatF32U  >(); }
EVENT(onInstI32TruncSatF64S  )() { addInst<I32TruncSatF64S  >(); }
EVENT(onInstI32TruncSatF64U  )() { addInst<I32TruncSatF64U  >(); }
EVENT(onInstI64TruncSatF32S  )() { addInst<I64TruncSatF32S  >(); }
EVENT(onInstI64TruncSatF32U  )() { addInst<I64TruncSatF32U  >(); }
EVENT(onInstI64TruncSatF64S  )() { addInst<I64TruncSatF64S  >(); }
EVENT(onInstI64TruncSatF64U  )() { addInst<I64TruncSatF64U  >(); }
// clang-format on
} // namespace parser
