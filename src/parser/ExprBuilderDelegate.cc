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

EVENT(onInstV128Load         )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load       >(Align, Offset);         }
EVENT(onInstV128Load8x8S     )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load8x8S   >(Align, Offset);         }
EVENT(onInstV128Load8x8U     )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load8x8U   >(Align, Offset);         }
EVENT(onInstV128Load16x4S    )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load16x4S  >(Align, Offset);         }
EVENT(onInstV128Load16x4U    )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load16x4U  >(Align, Offset);         }
EVENT(onInstV128Load32x2S    )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load32x2S  >(Align, Offset);         }
EVENT(onInstV128Load32x2U    )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load32x2U  >(Align, Offset);         }
EVENT(onInstV128Load8Splat   )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load8Splat >(Align, Offset);         }
EVENT(onInstV128Load16Splat  )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load16Splat>(Align, Offset);         }
EVENT(onInstV128Load32Splat  )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load32Splat>(Align, Offset);         }
EVENT(onInstV128Load64Splat  )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load64Splat>(Align, Offset);         }
EVENT(onInstV128Load32Zero   )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load32Zero >(Align, Offset);         }
EVENT(onInstV128Load64Zero   )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Load64Zero >(Align, Offset);         }
EVENT(onInstV128Load8Lane    )(std::uint32_t Align, std::uint32_t Offset, bytecode::SIMDLaneID LaneID) { addInst<V128Load8Lane  >(Align, Offset, LaneID); }
EVENT(onInstV128Load16Lane   )(std::uint32_t Align, std::uint32_t Offset, bytecode::SIMDLaneID LaneID) { addInst<V128Load16Lane >(Align, Offset, LaneID); }
EVENT(onInstV128Load32Lane   )(std::uint32_t Align, std::uint32_t Offset, bytecode::SIMDLaneID LaneID) { addInst<V128Load32Lane >(Align, Offset, LaneID); }
EVENT(onInstV128Load64Lane   )(std::uint32_t Align, std::uint32_t Offset, bytecode::SIMDLaneID LaneID) { addInst<V128Load64Lane >(Align, Offset, LaneID); }
EVENT(onInstV128Store        )(std::uint32_t Align, std::uint32_t Offset) { addInst<V128Store      >(Align, Offset);         }
EVENT(onInstV128Store8Lane   )(std::uint32_t Align, std::uint32_t Offset, bytecode::SIMDLaneID LaneID) { addInst<V128Store8Lane >(Align, Offset, LaneID); }
EVENT(onInstV128Store16Lane  )(std::uint32_t Align, std::uint32_t Offset, bytecode::SIMDLaneID LaneID) { addInst<V128Store16Lane>(Align, Offset, LaneID); }
EVENT(onInstV128Store32Lane  )(std::uint32_t Align, std::uint32_t Offset, bytecode::SIMDLaneID LaneID) { addInst<V128Store32Lane>(Align, Offset, LaneID); }
EVENT(onInstV128Store64Lane  )(std::uint32_t Align, std::uint32_t Offset, bytecode::SIMDLaneID LaneID) { addInst<V128Store64Lane>(Align, Offset, LaneID); }

EVENT(onInstV128Const        )(bytecode::V128Value Value             ) { addInst<V128Const>(Value);      }
EVENT(onInstI8x16Shuffle     )(bytecode::SIMDLaneIDVector<16> Indices) { addInst<I8x16Shuffle>(Indices); }
EVENT(onInstI8x16Swizzle     )() { addInst<I8x16Swizzle>(); }
EVENT(onInstI8x16Splat       )() { addInst<I8x16Splat  >(); }
EVENT(onInstI16x8Splat       )() { addInst<I16x8Splat  >(); }
EVENT(onInstI32x4Splat       )() { addInst<I32x4Splat  >(); }
EVENT(onInstI64x2Splat       )() { addInst<I64x2Splat  >(); }
EVENT(onInstF32x4Splat       )() { addInst<F32x4Splat  >(); }
EVENT(onInstF64x2Splat       )() { addInst<F64x2Splat  >(); }
EVENT(onInstI8x16ExtractLaneS)(bytecode::SIMDLaneID LaneID) { addInst<I8x16ExtractLaneS>(LaneID); }
EVENT(onInstI8x16ExtractLaneU)(bytecode::SIMDLaneID LaneID) { addInst<I8x16ExtractLaneU>(LaneID); }
EVENT(onInstI8x16ReplaceLane )(bytecode::SIMDLaneID LaneID) { addInst<I8x16ReplaceLane >(LaneID); }
EVENT(onInstI16x8ExtractLaneS)(bytecode::SIMDLaneID LaneID) { addInst<I16x8ExtractLaneS>(LaneID); }
EVENT(onInstI16x8ExtractLaneU)(bytecode::SIMDLaneID LaneID) { addInst<I16x8ExtractLaneU>(LaneID); }
EVENT(onInstI16x8ReplaceLane )(bytecode::SIMDLaneID LaneID) { addInst<I16x8ReplaceLane >(LaneID); }
EVENT(onInstI32x4ExtractLane )(bytecode::SIMDLaneID LaneID) { addInst<I32x4ExtractLane >(LaneID); }
EVENT(onInstI32x4ReplaceLane )(bytecode::SIMDLaneID LaneID) { addInst<I32x4ReplaceLane >(LaneID); }
EVENT(onInstI64x2ExtractLane )(bytecode::SIMDLaneID LaneID) { addInst<I64x2ExtractLane >(LaneID); }
EVENT(onInstI64x2ReplaceLane )(bytecode::SIMDLaneID LaneID) { addInst<I64x2ReplaceLane >(LaneID); }
EVENT(onInstF32x4ExtractLane )(bytecode::SIMDLaneID LaneID) { addInst<F32x4ExtractLane >(LaneID); }
EVENT(onInstF32x4ReplaceLane )(bytecode::SIMDLaneID LaneID) { addInst<F32x4ReplaceLane >(LaneID); }
EVENT(onInstF64x2ExtractLane )(bytecode::SIMDLaneID LaneID) { addInst<F64x2ExtractLane >(LaneID); }
EVENT(onInstF64x2ReplaceLane )(bytecode::SIMDLaneID LaneID) { addInst<F64x2ReplaceLane >(LaneID); }

EVENT(onInstV128Not          )() { addInst<V128Not      >(); }
EVENT(onInstV128And          )() { addInst<V128And      >(); }
EVENT(onInstV128AndNot       )() { addInst<V128AndNot   >(); }
EVENT(onInstV128Or           )() { addInst<V128Or       >(); }
EVENT(onInstV128Xor          )() { addInst<V128Xor      >(); }
EVENT(onInstV128BitSelect    )() { addInst<V128BitSelect>(); }
EVENT(onInstV128AnyTrue      )() { addInst<V128AnyTrue  >(); }

EVENT(onInstI8x16Eq                  )() { addInst<I8x16Eq                >(); }
EVENT(onInstI8x16Ne                  )() { addInst<I8x16Ne                >(); }
EVENT(onInstI8x16LtS                 )() { addInst<I8x16LtS               >(); }
EVENT(onInstI8x16LtU                 )() { addInst<I8x16LtU               >(); }
EVENT(onInstI8x16GtS                 )() { addInst<I8x16GtS               >(); }
EVENT(onInstI8x16GtU                 )() { addInst<I8x16GtU               >(); }
EVENT(onInstI8x16LeS                 )() { addInst<I8x16LeS               >(); }
EVENT(onInstI8x16LeU                 )() { addInst<I8x16LeU               >(); }
EVENT(onInstI8x16GeS                 )() { addInst<I8x16GeS               >(); }
EVENT(onInstI8x16GeU                 )() { addInst<I8x16GeU               >(); }
EVENT(onInstI8x16Abs                 )() { addInst<I8x16Abs               >(); }
EVENT(onInstI8x16Neg                 )() { addInst<I8x16Neg               >(); }
EVENT(onInstI8x16AllTrue             )() { addInst<I8x16AllTrue           >(); }
EVENT(onInstI8x16Bitmask             )() { addInst<I8x16Bitmask           >(); }
EVENT(onInstI8x16NarrowI16x8S        )() { addInst<I8x16NarrowI16x8S      >(); }
EVENT(onInstI8x16NarrowI16x8U        )() { addInst<I8x16NarrowI16x8U      >(); }
EVENT(onInstI8x16Shl                 )() { addInst<I8x16Shl               >(); }
EVENT(onInstI8x16ShrS                )() { addInst<I8x16ShrS              >(); }
EVENT(onInstI8x16ShrU                )() { addInst<I8x16ShrU              >(); }
EVENT(onInstI8x16Add                 )() { addInst<I8x16Add               >(); }
EVENT(onInstI8x16AddSatS             )() { addInst<I8x16AddSatS           >(); }
EVENT(onInstI8x16AddSatU             )() { addInst<I8x16AddSatU           >(); }
EVENT(onInstI8x16Sub                 )() { addInst<I8x16Sub               >(); }
EVENT(onInstI8x16SubSatS             )() { addInst<I8x16SubSatS           >(); }
EVENT(onInstI8x16SubSatU             )() { addInst<I8x16SubSatU           >(); }
EVENT(onInstI8x16MinS                )() { addInst<I8x16MinS              >(); }
EVENT(onInstI8x16MinU                )() { addInst<I8x16MinU              >(); }
EVENT(onInstI8x16MaxS                )() { addInst<I8x16MaxS              >(); }
EVENT(onInstI8x16MaxU                )() { addInst<I8x16MaxU              >(); }
EVENT(onInstI8x16AvgrU               )() { addInst<I8x16AvgrU             >(); }
EVENT(onInstI8x16Popcnt              )() { addInst<I8x16Popcnt            >(); }

EVENT(onInstI16x8Eq                  )() { addInst<I16x8Eq                >(); }
EVENT(onInstI16x8Ne                  )() { addInst<I16x8Ne                >(); }
EVENT(onInstI16x8LtS                 )() { addInst<I16x8LtS               >(); }
EVENT(onInstI16x8LtU                 )() { addInst<I16x8LtU               >(); }
EVENT(onInstI16x8GtS                 )() { addInst<I16x8GtS               >(); }
EVENT(onInstI16x8GtU                 )() { addInst<I16x8GtU               >(); }
EVENT(onInstI16x8LeS                 )() { addInst<I16x8LeS               >(); }
EVENT(onInstI16x8LeU                 )() { addInst<I16x8LeU               >(); }
EVENT(onInstI16x8GeS                 )() { addInst<I16x8GeS               >(); }
EVENT(onInstI16x8GeU                 )() { addInst<I16x8GeU               >(); }
EVENT(onInstI16x8Abs                 )() { addInst<I16x8Abs               >(); }
EVENT(onInstI16x8Neg                 )() { addInst<I16x8Neg               >(); }
EVENT(onInstI16x8AllTrue             )() { addInst<I16x8AllTrue           >(); }
EVENT(onInstI16x8Bitmask             )() { addInst<I16x8Bitmask           >(); }
EVENT(onInstI16x8NarrowI32x4S        )() { addInst<I16x8NarrowI32x4S      >(); }
EVENT(onInstI16x8NarrowI32x4U        )() { addInst<I16x8NarrowI32x4U      >(); }
EVENT(onInstI16x8ExtendLowI8x16S     )() { addInst<I16x8ExtendLowI8x16S   >(); }
EVENT(onInstI16x8ExtendHighI8x16S    )() { addInst<I16x8ExtendHighI8x16S  >(); }
EVENT(onInstI16x8ExtendLowI8x16U     )() { addInst<I16x8ExtendLowI8x16U   >(); }
EVENT(onInstI16x8ExtendHighI8x16U    )() { addInst<I16x8ExtendHighI8x16U  >(); }
EVENT(onInstI16x8Shl                 )() { addInst<I16x8Shl               >(); }
EVENT(onInstI16x8ShrS                )() { addInst<I16x8ShrS              >(); }
EVENT(onInstI16x8ShrU                )() { addInst<I16x8ShrU              >(); }
EVENT(onInstI16x8Add                 )() { addInst<I16x8Add               >(); }
EVENT(onInstI16x8AddSatS             )() { addInst<I16x8AddSatS           >(); }
EVENT(onInstI16x8AddSatU             )() { addInst<I16x8AddSatU           >(); }
EVENT(onInstI16x8Sub                 )() { addInst<I16x8Sub               >(); }
EVENT(onInstI16x8SubSatS             )() { addInst<I16x8SubSatS           >(); }
EVENT(onInstI16x8SubSatU             )() { addInst<I16x8SubSatU           >(); }
EVENT(onInstI16x8Mul                 )() { addInst<I16x8Mul               >(); }
EVENT(onInstI16x8MinS                )() { addInst<I16x8MinS              >(); }
EVENT(onInstI16x8MinU                )() { addInst<I16x8MinU              >(); }
EVENT(onInstI16x8MaxS                )() { addInst<I16x8MaxS              >(); }
EVENT(onInstI16x8MaxU                )() { addInst<I16x8MaxU              >(); }
EVENT(onInstI16x8AvgrU               )() { addInst<I16x8AvgrU             >(); }
EVENT(onInstI16x8Q15MulRSatS         )() { addInst<I16x8Q15MulRSatS       >(); }

EVENT(onInstI32x4Eq                  )() { addInst<I32x4Eq                >(); }
EVENT(onInstI32x4Ne                  )() { addInst<I32x4Ne                >(); }
EVENT(onInstI32x4LtS                 )() { addInst<I32x4LtS               >(); }
EVENT(onInstI32x4LtU                 )() { addInst<I32x4LtU               >(); }
EVENT(onInstI32x4GtS                 )() { addInst<I32x4GtS               >(); }
EVENT(onInstI32x4GtU                 )() { addInst<I32x4GtU               >(); }
EVENT(onInstI32x4LeS                 )() { addInst<I32x4LeS               >(); }
EVENT(onInstI32x4LeU                 )() { addInst<I32x4LeU               >(); }
EVENT(onInstI32x4GeS                 )() { addInst<I32x4GeS               >(); }
EVENT(onInstI32x4GeU                 )() { addInst<I32x4GeU               >(); }
EVENT(onInstI32x4Abs                 )() { addInst<I32x4Abs               >(); }
EVENT(onInstI32x4Neg                 )() { addInst<I32x4Neg               >(); }
EVENT(onInstI32x4AllTrue             )() { addInst<I32x4AllTrue           >(); }
EVENT(onInstI32x4Bitmask             )() { addInst<I32x4Bitmask           >(); }
EVENT(onInstI32x4ExtendLowI16x8S     )() { addInst<I32x4ExtendLowI16x8S   >(); }
EVENT(onInstI32x4ExtendHighI16x8S    )() { addInst<I32x4ExtendHighI16x8S  >(); }
EVENT(onInstI32x4ExtendLowI16x8U     )() { addInst<I32x4ExtendLowI16x8U   >(); }
EVENT(onInstI32x4ExtendHighI16x8U    )() { addInst<I32x4ExtendHighI16x8U  >(); }
EVENT(onInstI32x4Shl                 )() { addInst<I32x4Shl               >(); }
EVENT(onInstI32x4ShrS                )() { addInst<I32x4ShrS              >(); }
EVENT(onInstI32x4ShrU                )() { addInst<I32x4ShrU              >(); }
EVENT(onInstI32x4Add                 )() { addInst<I32x4Add               >(); }
EVENT(onInstI32x4Sub                 )() { addInst<I32x4Sub               >(); }
EVENT(onInstI32x4Mul                 )() { addInst<I32x4Mul               >(); }
EVENT(onInstI32x4MinS                )() { addInst<I32x4MinS              >(); }
EVENT(onInstI32x4MinU                )() { addInst<I32x4MinU              >(); }
EVENT(onInstI32x4MaxS                )() { addInst<I32x4MaxS              >(); }
EVENT(onInstI32x4MaxU                )() { addInst<I32x4MaxU              >(); }
EVENT(onInstI32x4DotI16x8S           )() { addInst<I32x4DotI16x8S         >(); }

EVENT(onInstI64x2Eq                  )() { addInst<I64x2Eq                >(); }
EVENT(onInstI64x2Ne                  )() { addInst<I64x2Ne                >(); }
EVENT(onInstI64x2LtS                 )() { addInst<I64x2LtS               >(); }
EVENT(onInstI64x2GtS                 )() { addInst<I64x2GtS               >(); }
EVENT(onInstI64x2LeS                 )() { addInst<I64x2LeS               >(); }
EVENT(onInstI64x2GeS                 )() { addInst<I64x2GeS               >(); }
EVENT(onInstI64x2AllTrue             )() { addInst<I64x2AllTrue           >(); }
EVENT(onInstI64x2Abs                 )() { addInst<I64x2Abs               >(); }
EVENT(onInstI64x2Neg                 )() { addInst<I64x2Neg               >(); }
EVENT(onInstI64x2Bitmask             )() { addInst<I64x2Bitmask           >(); }
EVENT(onInstI64x2ExtendLowI32x4S     )() { addInst<I64x2ExtendLowI32x4S   >(); }
EVENT(onInstI64x2ExtendHighI32x4S    )() { addInst<I64x2ExtendHighI32x4S  >(); }
EVENT(onInstI64x2ExtendLowI32x4U     )() { addInst<I64x2ExtendLowI32x4U   >(); }
EVENT(onInstI64x2ExtendHighI32x4U    )() { addInst<I64x2ExtendHighI32x4U  >(); }
EVENT(onInstI64x2Shl                 )() { addInst<I64x2Shl               >(); }
EVENT(onInstI64x2ShrS                )() { addInst<I64x2ShrS              >(); }
EVENT(onInstI64x2ShrU                )() { addInst<I64x2ShrU              >(); }
EVENT(onInstI64x2Add                 )() { addInst<I64x2Add               >(); }
EVENT(onInstI64x2Sub                 )() { addInst<I64x2Sub               >(); }
EVENT(onInstI64x2Mul                 )() { addInst<I64x2Mul               >(); }

EVENT(onInstF32x4Eq                  )() { addInst<F32x4Eq                >(); }
EVENT(onInstF32x4Ne                  )() { addInst<F32x4Ne                >(); }
EVENT(onInstF32x4Lt                  )() { addInst<F32x4Lt                >(); }
EVENT(onInstF32x4Gt                  )() { addInst<F32x4Gt                >(); }
EVENT(onInstF32x4Le                  )() { addInst<F32x4Le                >(); }
EVENT(onInstF32x4Ge                  )() { addInst<F32x4Ge                >(); }
EVENT(onInstF32x4Ceil                )() { addInst<F32x4Ceil              >(); }
EVENT(onInstF32x4Floor               )() { addInst<F32x4Floor             >(); }
EVENT(onInstF32x4Trunc               )() { addInst<F32x4Trunc             >(); }
EVENT(onInstF32x4Nearest             )() { addInst<F32x4Nearest           >(); }
EVENT(onInstF32x4Abs                 )() { addInst<F32x4Abs               >(); }
EVENT(onInstF32x4Neg                 )() { addInst<F32x4Neg               >(); }
EVENT(onInstF32x4Sqrt                )() { addInst<F32x4Sqrt              >(); }
EVENT(onInstF32x4Add                 )() { addInst<F32x4Add               >(); }
EVENT(onInstF32x4Sub                 )() { addInst<F32x4Sub               >(); }
EVENT(onInstF32x4Mul                 )() { addInst<F32x4Mul               >(); }
EVENT(onInstF32x4Div                 )() { addInst<F32x4Div               >(); }
EVENT(onInstF32x4Min                 )() { addInst<F32x4Min               >(); }
EVENT(onInstF32x4Max                 )() { addInst<F32x4Max               >(); }
EVENT(onInstF32x4PMin                )() { addInst<F32x4PMin              >(); }
EVENT(onInstF32x4PMax                )() { addInst<F32x4PMax              >(); }

EVENT(onInstF64x2Eq                  )() { addInst<F64x2Eq                >(); }
EVENT(onInstF64x2Ne                  )() { addInst<F64x2Ne                >(); }
EVENT(onInstF64x2Lt                  )() { addInst<F64x2Lt                >(); }
EVENT(onInstF64x2Gt                  )() { addInst<F64x2Gt                >(); }
EVENT(onInstF64x2Le                  )() { addInst<F64x2Le                >(); }
EVENT(onInstF64x2Ge                  )() { addInst<F64x2Ge                >(); }
EVENT(onInstF64x2Ceil                )() { addInst<F64x2Ceil              >(); }
EVENT(onInstF64x2Floor               )() { addInst<F64x2Floor             >(); }
EVENT(onInstF64x2Trunc               )() { addInst<F64x2Trunc             >(); }
EVENT(onInstF64x2Nearest             )() { addInst<F64x2Nearest           >(); }
EVENT(onInstF64x2Abs                 )() { addInst<F64x2Abs               >(); }
EVENT(onInstF64x2Neg                 )() { addInst<F64x2Neg               >(); }
EVENT(onInstF64x2Sqrt                )() { addInst<F64x2Sqrt              >(); }
EVENT(onInstF64x2Add                 )() { addInst<F64x2Add               >(); }
EVENT(onInstF64x2Sub                 )() { addInst<F64x2Sub               >(); }
EVENT(onInstF64x2Mul                 )() { addInst<F64x2Mul               >(); }
EVENT(onInstF64x2Div                 )() { addInst<F64x2Div               >(); }
EVENT(onInstF64x2Min                 )() { addInst<F64x2Min               >(); }
EVENT(onInstF64x2Max                 )() { addInst<F64x2Max               >(); }
EVENT(onInstF64x2PMin                )() { addInst<F64x2PMin              >(); }
EVENT(onInstF64x2PMax                )() { addInst<F64x2PMax              >(); }

EVENT(onInstI32x4TruncSatF32x4S      )() { addInst<I32x4TruncSatF32x4S    >(); }
EVENT(onInstI32x4TruncSatF32x4U      )() { addInst<I32x4TruncSatF32x4U    >(); }
EVENT(onInstF32x4ConvertI32x4S       )() { addInst<F32x4ConvertI32x4S     >(); }
EVENT(onInstF32x4ConvertI32x4U       )() { addInst<F32x4ConvertI32x4U     >(); }
EVENT(onInstF64x2ConvertLowI32x4S    )() { addInst<F64x2ConvertLowI32x4S  >(); }
EVENT(onInstF64x2ConvertLowI32x4U    )() { addInst<F64x2ConvertLowI32x4U  >(); }
EVENT(onInstI32x4TruncSatF64x2SZero  )() { addInst<I32x4TruncSatF64x2SZero>(); }
EVENT(onInstI32x4TruncSatF64x2UZero  )() { addInst<I32x4TruncSatF64x2UZero>(); }
EVENT(onInstF32x4DemoteF64x2Zero     )() { addInst<F32x4DemoteF64x2Zero   >(); }
EVENT(onInstF64x2PromoteLowF32x4     )() { addInst<F64x2PromoteLowF32x4   >(); }

EVENT(onInstI16x8ExtMulLowI8x16S     )() { addInst<I16x8ExtMulLowI8x16S     >(); }
EVENT(onInstI16x8ExtMulHighI8x16S    )() { addInst<I16x8ExtMulHighI8x16S    >(); }
EVENT(onInstI16x8ExtMulLowI8x16U     )() { addInst<I16x8ExtMulLowI8x16U     >(); }
EVENT(onInstI16x8ExtMulHighI8x16U    )() { addInst<I16x8ExtMulHighI8x16U    >(); }
EVENT(onInstI32x4ExtMulLowI16x8S     )() { addInst<I32x4ExtMulLowI16x8S     >(); }
EVENT(onInstI32x4ExtMulHighI16x8S    )() { addInst<I32x4ExtMulHighI16x8S    >(); }
EVENT(onInstI32x4ExtMulLowI16x8U     )() { addInst<I32x4ExtMulLowI16x8U     >(); }
EVENT(onInstI32x4ExtMulHighI16x8U    )() { addInst<I32x4ExtMulHighI16x8U    >(); }
EVENT(onInstI64x2ExtMulLowI32x4S     )() { addInst<I64x2ExtMulLowI32x4S     >(); }
EVENT(onInstI64x2ExtMulHighI32x4S    )() { addInst<I64x2ExtMulHighI32x4S    >(); }
EVENT(onInstI64x2ExtMulLowI32x4U     )() { addInst<I64x2ExtMulLowI32x4U     >(); }
EVENT(onInstI64x2ExtMulHighI32x4U    )() { addInst<I64x2ExtMulHighI32x4U    >(); }
EVENT(onInstI16x8ExtAddPairwiseI8x16S)() { addInst<I16x8ExtAddPairwiseI8x16S>(); }
EVENT(onInstI16x8ExtAddPairwiseI8x16U)() { addInst<I16x8ExtAddPairwiseI8x16U>(); }
EVENT(onInstI32x4ExtAddPairwiseI16x8S)() { addInst<I32x4ExtAddPairwiseI16x8S>(); }
EVENT(onInstI32x4ExtAddPairwiseI16x8U)() { addInst<I32x4ExtAddPairwiseI16x8U>(); }

// clang-format on
#undef EVENT
} // namespace parser
