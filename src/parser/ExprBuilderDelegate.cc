#include "ExprBuilderDelegate.h"

#include <range/v3/all.hpp>

namespace parser {
using namespace bytecode::instructions;
#define EVENT(Name) void ExprBuilderDelegate::Name

template <bytecode::instruction T>
T *ExprBuilderDelegate::getCurrentScopeEnclosingInst() {
  assert(!Scopes.empty() && !Scopes.top().empty());
  bytecode::Instruction *Instruction = Scopes.top().back();
  assert(bytecode::is_a<T>(Instruction));
  return static_cast<T *>(Instruction);
}

EVENT(enterExpression)() { CExpr.clear(); }
EVENT(exitExpression)() {}

EVENT(onInstUnreachable)() { CExpr.emplace_back<Unreachable>(); }
EVENT(onInstNop)() { CExpr.emplace_back<Nop>(); }
EVENT(enterInstBlock)(bytecode::BlockResultType Type) {
  CExpr.emplace_back<Block>(Type, bytecode::Expression{});
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
  CExpr.emplace_back<Loop>(Type, bytecode::Expression{});
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
  CExpr.emplace_back<If>(Type, bytecode::Expression{}, std::nullopt);
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
EVENT(onInstBr)(bytecode::LabelIDX Index) { CExpr.emplace_back<Br>(Index); }
EVENT(onInstBrIf)(bytecode::LabelIDX Index) { CExpr.emplace_back<BrIf>(Index); }
EVENT(onInstBrTable)
(bytecode::LabelIDX DefaultTarget,
 std::span<bytecode::LabelIDX const> Targets) {
  std::vector<bytecode::LabelIDX> Targets_;
  Targets_.reserve(Targets.size());
  ranges::copy(Targets, ranges::back_inserter(Targets_));
  CExpr.emplace_back<BrTable>(std::move(Targets_), DefaultTarget);
}
EVENT(onInstReturn)() { CExpr.emplace_back<Return>(); }
EVENT(onInstCall)(bytecode::FuncIDX IDX) { CExpr.emplace_back<Call>(IDX); }
EVENT(onInstCallIndirect)(bytecode::TypeIDX IDX) {
  CExpr.emplace_back<CallIndirect>(IDX);
}

EVENT(onInstDrop)() { CExpr.emplace_back<Drop>(); }
EVENT(onInstSelect)() { CExpr.emplace_back<Select>(); }

#define LOCAL_EVENT(Name, InstName)                                            \
  EVENT(Name)(bytecode::LocalIDX IDX) { CExpr.emplace_back<InstName>(IDX); }
LOCAL_EVENT(onInstLocalGet, LocalGet)
LOCAL_EVENT(onInstLocalSet, LocalSet)
LOCAL_EVENT(onInstLocalTee, LocalTee)
#undef LOCAL_EVENT

#define GLOBAL_EVENT(Name, InstName)                                           \
  EVENT(Name)(bytecode::GlobalIDX IDX) { CExpr.emplace_back<InstName>(IDX); }
GLOBAL_EVENT(onInstGlobalGet, GlobalGet)
GLOBAL_EVENT(onInstGlobalSet, GlobalSet)
#undef GLOBAL_EVENT

#define MEMORY_EVENT(Name, InstName)                                           \
  EVENT(Name)(std::uint32_t Align, std::uint32_t Offset) {                     \
    CExpr.emplace_back<InstName>(Align, Offset);                               \
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
EVENT(onInstMemorySize)() { CExpr.emplace_back<MemorySize>(); }
EVENT(onInstMemoryGrow)() { CExpr.emplace_back<MemoryGrow>(); }

EVENT(onInstI32Const)(std::int32_t N) { CExpr.emplace_back<I32Const>(N); }
EVENT(onInstI64Const)(std::int64_t N) { CExpr.emplace_back<I64Const>(N); }
EVENT(onInstF32Const)(float N) { CExpr.emplace_back<F32Const>(N); }
EVENT(onInstF64Const)(double N) { CExpr.emplace_back<F64Const>(N); }

// clang-format off
EVENT(onInstI32Eqz           )() { CExpr.emplace_back<I32Eqz           >(); }
EVENT(onInstI32Eq            )() { CExpr.emplace_back<I32Eq            >(); }
EVENT(onInstI32Ne            )() { CExpr.emplace_back<I32Ne            >(); }
EVENT(onInstI32LtS           )() { CExpr.emplace_back<I32LtS           >(); }
EVENT(onInstI32LtU           )() { CExpr.emplace_back<I32LtU           >(); }
EVENT(onInstI32GtS           )() { CExpr.emplace_back<I32GtS           >(); }
EVENT(onInstI32GtU           )() { CExpr.emplace_back<I32GtU           >(); }
EVENT(onInstI32LeS           )() { CExpr.emplace_back<I32LeS           >(); }
EVENT(onInstI32LeU           )() { CExpr.emplace_back<I32LeU           >(); }
EVENT(onInstI32GeS           )() { CExpr.emplace_back<I32GeS           >(); }
EVENT(onInstI32GeU           )() { CExpr.emplace_back<I32GeU           >(); }

EVENT(onInstI64Eqz           )() { CExpr.emplace_back<I64Eqz           >(); }
EVENT(onInstI64Eq            )() { CExpr.emplace_back<I64Eq            >(); }
EVENT(onInstI64Ne            )() { CExpr.emplace_back<I64Ne            >(); }
EVENT(onInstI64LtS           )() { CExpr.emplace_back<I64LtS           >(); }
EVENT(onInstI64LtU           )() { CExpr.emplace_back<I64LtU           >(); }
EVENT(onInstI64GtS           )() { CExpr.emplace_back<I64GtS           >(); }
EVENT(onInstI64GtU           )() { CExpr.emplace_back<I64GtU           >(); }
EVENT(onInstI64LeS           )() { CExpr.emplace_back<I64LeS           >(); }
EVENT(onInstI64LeU           )() { CExpr.emplace_back<I64LeU           >(); }
EVENT(onInstI64GeS           )() { CExpr.emplace_back<I64GeS           >(); }
EVENT(onInstI64GeU           )() { CExpr.emplace_back<I64GeU           >(); }

EVENT(onInstF32Eq            )() { CExpr.emplace_back<F32Eq            >(); }
EVENT(onInstF32Ne            )() { CExpr.emplace_back<F32Ne            >(); }
EVENT(onInstF32Lt            )() { CExpr.emplace_back<F32Lt            >(); }
EVENT(onInstF32Gt            )() { CExpr.emplace_back<F32Gt            >(); }
EVENT(onInstF32Le            )() { CExpr.emplace_back<F32Le            >(); }
EVENT(onInstF32Ge            )() { CExpr.emplace_back<F32Ge            >(); }

EVENT(onInstF64Eq            )() { CExpr.emplace_back<F64Eq            >(); }
EVENT(onInstF64Ne            )() { CExpr.emplace_back<F64Ne            >(); }
EVENT(onInstF64Lt            )() { CExpr.emplace_back<F64Lt            >(); }
EVENT(onInstF64Gt            )() { CExpr.emplace_back<F64Gt            >(); }
EVENT(onInstF64Le            )() { CExpr.emplace_back<F64Le            >(); }
EVENT(onInstF64Ge            )() { CExpr.emplace_back<F64Ge            >(); }

EVENT(onInstI32Clz           )() { CExpr.emplace_back<I32Clz           >(); }
EVENT(onInstI32Ctz           )() { CExpr.emplace_back<I32Ctz           >(); }
EVENT(onInstI32Popcnt        )() { CExpr.emplace_back<I32Popcnt        >(); }
EVENT(onInstI32Add           )() { CExpr.emplace_back<I32Add           >(); }
EVENT(onInstI32Sub           )() { CExpr.emplace_back<I32Sub           >(); }
EVENT(onInstI32Mul           )() { CExpr.emplace_back<I32Mul           >(); }
EVENT(onInstI32DivS          )() { CExpr.emplace_back<I32DivS          >(); }
EVENT(onInstI32DivU          )() { CExpr.emplace_back<I32DivU          >(); }
EVENT(onInstI32RemS          )() { CExpr.emplace_back<I32RemS          >(); }
EVENT(onInstI32RemU          )() { CExpr.emplace_back<I32RemU          >(); }
EVENT(onInstI32And           )() { CExpr.emplace_back<I32And           >(); }
EVENT(onInstI32Or            )() { CExpr.emplace_back<I32Or            >(); }
EVENT(onInstI32Xor           )() { CExpr.emplace_back<I32Xor           >(); }
EVENT(onInstI32Shl           )() { CExpr.emplace_back<I32Shl           >(); }
EVENT(onInstI32ShrS          )() { CExpr.emplace_back<I32ShrS          >(); }
EVENT(onInstI32ShrU          )() { CExpr.emplace_back<I32ShrU          >(); }
EVENT(onInstI32Rotl          )() { CExpr.emplace_back<I32Rotl          >(); }
EVENT(onInstI32Rotr          )() { CExpr.emplace_back<I32Rotr          >(); }

EVENT(onInstI64Clz           )() { CExpr.emplace_back<I64Clz           >(); }
EVENT(onInstI64Ctz           )() { CExpr.emplace_back<I64Ctz           >(); }
EVENT(onInstI64Popcnt        )() { CExpr.emplace_back<I64Popcnt        >(); }
EVENT(onInstI64Add           )() { CExpr.emplace_back<I64Add           >(); }
EVENT(onInstI64Sub           )() { CExpr.emplace_back<I64Sub           >(); }
EVENT(onInstI64Mul           )() { CExpr.emplace_back<I64Mul           >(); }
EVENT(onInstI64DivS          )() { CExpr.emplace_back<I64DivS          >(); }
EVENT(onInstI64DivU          )() { CExpr.emplace_back<I64DivU          >(); }
EVENT(onInstI64RemS          )() { CExpr.emplace_back<I64RemS          >(); }
EVENT(onInstI64RemU          )() { CExpr.emplace_back<I64RemU          >(); }
EVENT(onInstI64And           )() { CExpr.emplace_back<I64And           >(); }
EVENT(onInstI64Or            )() { CExpr.emplace_back<I64Or            >(); }
EVENT(onInstI64Xor           )() { CExpr.emplace_back<I64Xor           >(); }
EVENT(onInstI64Shl           )() { CExpr.emplace_back<I64Shl           >(); }
EVENT(onInstI64ShrS          )() { CExpr.emplace_back<I64ShrS          >(); }
EVENT(onInstI64ShrU          )() { CExpr.emplace_back<I64ShrU          >(); }
EVENT(onInstI64Rotl          )() { CExpr.emplace_back<I64Rotl          >(); }
EVENT(onInstI64Rotr          )() { CExpr.emplace_back<I64Rotr          >(); }

EVENT(onInstF32Abs           )() { CExpr.emplace_back<F32Abs           >(); }
EVENT(onInstF32Neg           )() { CExpr.emplace_back<F32Neg           >(); }
EVENT(onInstF32Ceil          )() { CExpr.emplace_back<F32Ceil          >(); }
EVENT(onInstF32Floor         )() { CExpr.emplace_back<F32Floor         >(); }
EVENT(onInstF32Trunc         )() { CExpr.emplace_back<F32Trunc         >(); }
EVENT(onInstF32Nearest       )() { CExpr.emplace_back<F32Nearest       >(); }
EVENT(onInstF32Sqrt          )() { CExpr.emplace_back<F32Sqrt          >(); }
EVENT(onInstF32Add           )() { CExpr.emplace_back<F32Add           >(); }
EVENT(onInstF32Sub           )() { CExpr.emplace_back<F32Sub           >(); }
EVENT(onInstF32Mul           )() { CExpr.emplace_back<F32Mul           >(); }
EVENT(onInstF32Div           )() { CExpr.emplace_back<F32Div           >(); }
EVENT(onInstF32Min           )() { CExpr.emplace_back<F32Min           >(); }
EVENT(onInstF32Max           )() { CExpr.emplace_back<F32Max           >(); }
EVENT(onInstF32CopySign      )() { CExpr.emplace_back<F32CopySign      >(); }

EVENT(onInstF64Abs           )() { CExpr.emplace_back<F64Abs           >(); }
EVENT(onInstF64Neg           )() { CExpr.emplace_back<F64Neg           >(); }
EVENT(onInstF64Ceil          )() { CExpr.emplace_back<F64Ceil          >(); }
EVENT(onInstF64Floor         )() { CExpr.emplace_back<F64Floor         >(); }
EVENT(onInstF64Trunc         )() { CExpr.emplace_back<F64Trunc         >(); }
EVENT(onInstF64Nearest       )() { CExpr.emplace_back<F64Nearest       >(); }
EVENT(onInstF64Sqrt          )() { CExpr.emplace_back<F64Sqrt          >(); }
EVENT(onInstF64Add           )() { CExpr.emplace_back<F64Add           >(); }
EVENT(onInstF64Sub           )() { CExpr.emplace_back<F64Sub           >(); }
EVENT(onInstF64Mul           )() { CExpr.emplace_back<F64Mul           >(); }
EVENT(onInstF64Div           )() { CExpr.emplace_back<F64Div           >(); }
EVENT(onInstF64Min           )() { CExpr.emplace_back<F64Min           >(); }
EVENT(onInstF64Max           )() { CExpr.emplace_back<F64Max           >(); }
EVENT(onInstF64CopySign      )() { CExpr.emplace_back<F64CopySign      >(); }

EVENT(onInstI32WrapI64       )() { CExpr.emplace_back<I32WrapI64       >(); }
EVENT(onInstI32TruncF32S     )() { CExpr.emplace_back<I32TruncF32S     >(); }
EVENT(onInstI32TruncF32U     )() { CExpr.emplace_back<I32TruncF32U     >(); }
EVENT(onInstI32TruncF64S     )() { CExpr.emplace_back<I32TruncF64S     >(); }
EVENT(onInstI32TruncF64U     )() { CExpr.emplace_back<I32TruncF64U     >(); }
EVENT(onInstI64ExtendI32S    )() { CExpr.emplace_back<I64ExtendI32S    >(); }
EVENT(onInstI64ExtendI32U    )() { CExpr.emplace_back<I64ExtendI32U    >(); }
EVENT(onInstI64TruncF32S     )() { CExpr.emplace_back<I64TruncF32S     >(); }
EVENT(onInstI64TruncF32U     )() { CExpr.emplace_back<I64TruncF32U     >(); }
EVENT(onInstI64TruncF64S     )() { CExpr.emplace_back<I64TruncF64S     >(); }
EVENT(onInstI64TruncF64U     )() { CExpr.emplace_back<I64TruncF64U     >(); }
EVENT(onInstF32ConvertI32S   )() { CExpr.emplace_back<F32ConvertI32S   >(); }
EVENT(onInstF32ConvertI32U   )() { CExpr.emplace_back<F32ConvertI32U   >(); }
EVENT(onInstF32ConvertI64S   )() { CExpr.emplace_back<F32ConvertI64S   >(); }
EVENT(onInstF32ConvertI64U   )() { CExpr.emplace_back<F32ConvertI64U   >(); }
EVENT(onInstF32DemoteF64     )() { CExpr.emplace_back<F32DemoteF64     >(); }
EVENT(onInstF64ConvertI32S   )() { CExpr.emplace_back<F64ConvertI32S   >(); }
EVENT(onInstF64ConvertI32U   )() { CExpr.emplace_back<F64ConvertI32U   >(); }
EVENT(onInstF64ConvertI64S   )() { CExpr.emplace_back<F64ConvertI64S   >(); }
EVENT(onInstF64ConvertI64U   )() { CExpr.emplace_back<F64ConvertI64U   >(); }
EVENT(onInstF64PromoteF32    )() { CExpr.emplace_back<F64PromoteF32    >(); }
EVENT(onInstI32ReinterpretF32)() { CExpr.emplace_back<I32ReinterpretF32>(); }
EVENT(onInstI64ReinterpretF64)() { CExpr.emplace_back<I64ReinterpretF64>(); }
EVENT(onInstF32ReinterpretI32)() { CExpr.emplace_back<F32ReinterpretI32>(); }
EVENT(onInstF64ReinterpretI64)() { CExpr.emplace_back<F64ReinterpretI64>(); }

EVENT(onInstI32Extend8S      )() { CExpr.emplace_back<I32Extend8S      >(); }
EVENT(onInstI32Extend16S     )() { CExpr.emplace_back<I32Extend16S     >(); }
EVENT(onInstI64Extend8S      )() { CExpr.emplace_back<I64Extend8S      >(); }
EVENT(onInstI64Extend16S     )() { CExpr.emplace_back<I64Extend16S     >(); }
EVENT(onInstI64Extend32S     )() { CExpr.emplace_back<I64Extend32S     >(); }

EVENT(onInstI32TruncSatF32S  )() { CExpr.emplace_back<I32TruncSatF32S  >(); }
EVENT(onInstI32TruncSatF32U  )() { CExpr.emplace_back<I32TruncSatF32U  >(); }
EVENT(onInstI32TruncSatF64S  )() { CExpr.emplace_back<I32TruncSatF64S  >(); }
EVENT(onInstI32TruncSatF64U  )() { CExpr.emplace_back<I32TruncSatF64U  >(); }
EVENT(onInstI64TruncSatF32S  )() { CExpr.emplace_back<I64TruncSatF32S  >(); }
EVENT(onInstI64TruncSatF32U  )() { CExpr.emplace_back<I64TruncSatF32U  >(); }
EVENT(onInstI64TruncSatF64S  )() { CExpr.emplace_back<I64TruncSatF64S  >(); }
EVENT(onInstI64TruncSatF64U  )() { CExpr.emplace_back<I64TruncSatF64U  >(); }
// clang-format on
} // namespace parser