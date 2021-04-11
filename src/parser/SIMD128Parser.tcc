#ifndef SABLE_INCLUDE_GUARD_PARSER_SIMD_PARSER
#define SABLE_INCLUDE_GUARD_PARSER_SIMD_PARSER

#include "Delegate.h"
#include "Reader.h"

namespace parser {
struct SIMDParserBase {
  static constexpr std::byte Prefix = std::byte(0xfd);
};

template <delegate DelegateType> struct SIMDParser : SIMDParserBase {
  DelegateType &Delegate;

  explicit SIMDParser(DelegateType &Delegate_) : Delegate(Delegate_) {}

  template <reader ReaderBase> void operator()(WASMReader<ReaderBase> &Reader) {
    auto SIMDOpcode = Reader.read();
    switch (static_cast<unsigned>(SIMDOpcode)) {
#define SABLE_MEMORY_INSTRUCTION(Opcode, Name)                                 \
  case Opcode: {                                                               \
    auto Align = Reader.readULEB128Int32();                                    \
    auto Offset = Reader.readULEB128Int32();                                   \
    Delegate.onInst##Name(Align, Offset);                                      \
    break;                                                                     \
  }
      SABLE_MEMORY_INSTRUCTION(0x00, V128Load)
      SABLE_MEMORY_INSTRUCTION(0x01, V128Load8x8S)
      SABLE_MEMORY_INSTRUCTION(0x02, V128Load8x8U)
      SABLE_MEMORY_INSTRUCTION(0x03, V128Load16x4S)
      SABLE_MEMORY_INSTRUCTION(0x04, V128Load16x4U)
      SABLE_MEMORY_INSTRUCTION(0x05, V128Load32x2S)
      SABLE_MEMORY_INSTRUCTION(0x06, V128Load32x2U)
      SABLE_MEMORY_INSTRUCTION(0x07, V128Load8Splat)
      SABLE_MEMORY_INSTRUCTION(0x08, V128Load16Splat)
      SABLE_MEMORY_INSTRUCTION(0x09, V128Load32Splat)
      SABLE_MEMORY_INSTRUCTION(0x0a, V128Load64Splat)
      SABLE_MEMORY_INSTRUCTION(0x0b, V128Store)
#undef SABLE_MEMORY_INSTRUCTION
    case 0x0c: {
      auto ImmediateBytes = Reader.read(16);
      Delegate.onInstV128Const(bytecode::V128Value(ImmediateBytes));
      break;
    }
    case 0x0d: {
      std::array<bytecode::SIMDLaneID, 16> SIMDLaneIndices{};
      for (std::size_t I = 0; I < 16; ++I)
        SIMDLaneIndices[I] = static_cast<bytecode::SIMDLaneID>(Reader.read());
      Delegate.onInstI8x16Shuffle(SIMDLaneIndices);
      break;
    }
    case 0x0e: Delegate.onInstI8x16Swizzle(); break;
    case 0x0f: Delegate.onInstI8x16Splat(); break;
    case 0x10: Delegate.onInstI16x8Splat(); break;
    case 0x11: Delegate.onInstI32x4Splat(); break;
    case 0x12: Delegate.onInstI64x2Splat(); break;
    case 0x13: Delegate.onInstF32x4Splat(); break;
    case 0x14: Delegate.onInstF64x2Splat(); break;
#define SABLE_LANE_INSTRUCTION(Opcode, Name)                                   \
  case Opcode: {                                                               \
    auto LaneIndex = static_cast<bytecode::SIMDLaneID>(Reader.read());         \
    Delegate.onInst##Name(LaneIndex);                                          \
    break;                                                                     \
  }
      SABLE_LANE_INSTRUCTION(0x15, I8x16ExtractLaneS)
      SABLE_LANE_INSTRUCTION(0x16, I8x16ExtractLaneU)
      SABLE_LANE_INSTRUCTION(0x17, I8x16ReplaceLane)
      SABLE_LANE_INSTRUCTION(0x18, I16x8ExtractLaneS)
      SABLE_LANE_INSTRUCTION(0x19, I16x8ExtractLaneU)
      SABLE_LANE_INSTRUCTION(0x1a, I16x8ReplaceLane)
      SABLE_LANE_INSTRUCTION(0x1b, I32x4ExtractLane)
      SABLE_LANE_INSTRUCTION(0x1c, I32x4ReplaceLane)
      SABLE_LANE_INSTRUCTION(0x1d, I64x2ExtractLane)
      SABLE_LANE_INSTRUCTION(0x1e, I64x2ReplaceLane)
      SABLE_LANE_INSTRUCTION(0x1f, F32x4ExtractLane)
      SABLE_LANE_INSTRUCTION(0x20, F32x4ReplaceLane)
      SABLE_LANE_INSTRUCTION(0x21, F64x2ExtractLane)
      SABLE_LANE_INSTRUCTION(0x22, F64x2ReplaceLane)
#undef SABLE_LANE_INSTRUCTION
    case 0x23: Delegate.onInstI8x16Eq(); break;
    case 0x24: Delegate.onInstI8x16Ne(); break;
    case 0x25: Delegate.onInstI8x16LtS(); break;
    case 0x26: Delegate.onInstI8x16LtU(); break;
    case 0x27: Delegate.onInstI8x16GtS(); break;
    case 0x28: Delegate.onInstI8x16GtU(); break;
    case 0x29: Delegate.onInstI8x16LeS(); break;
    case 0x2a: Delegate.onInstI8x16LeU(); break;
    case 0x2b: Delegate.onInstI8x16GeS(); break;
    case 0x2c: Delegate.onInstI8x16GeU(); break;
    case 0x2d: Delegate.onInstI16x8Eq(); break;
    case 0x2e: Delegate.onInstI16x8Ne(); break;
    case 0x2f: Delegate.onInstI16x8LtS(); break;
    case 0x30: Delegate.onInstI16x8LtU(); break;
    case 0x31: Delegate.onInstI16x8GtS(); break;
    case 0x32: Delegate.onInstI16x8GtU(); break;
    case 0x33: Delegate.onInstI16x8LeS(); break;
    case 0x34: Delegate.onInstI16x8LeU(); break;
    case 0x35: Delegate.onInstI16x8GeS(); break;
    case 0x36: Delegate.onInstI16x8GeU(); break;
    case 0x37: Delegate.onInstI32x4Eq(); break;
    case 0x38: Delegate.onInstI32x4Ne(); break;
    case 0x39: Delegate.onInstI32x4LtS(); break;
    case 0x3a: Delegate.onInstI32x4LtU(); break;
    case 0x3b: Delegate.onInstI32x4GtS(); break;
    case 0x3c: Delegate.onInstI32x4GtU(); break;
    case 0x3d: Delegate.onInstI32x4LeS(); break;
    case 0x3e: Delegate.onInstI32x4LeU(); break;
    case 0x3f: Delegate.onInstI32x4GeS(); break;
    case 0x40: Delegate.onInstI32x4GeU(); break;
    case 0x41: Delegate.onInstF32x4Eq(); break;
    case 0x42: Delegate.onInstF32x4Ne(); break;
    case 0x43: Delegate.onInstF32x4Lt(); break;
    case 0x44: Delegate.onInstF32x4Gt(); break;
    case 0x45: Delegate.onInstF32x4Le(); break;
    case 0x46: Delegate.onInstF32x4Ge(); break;
    case 0x47: Delegate.onInstF64x2Eq(); break;
    case 0x48: Delegate.onInstF64x2Ne(); break;
    case 0x49: Delegate.onInstF64x2Lt(); break;
    case 0x4a: Delegate.onInstF64x2Gt(); break;
    case 0x4b: Delegate.onInstF64x2Le(); break;
    case 0x4c: Delegate.onInstF64x2Ge(); break;
    case 0x4d: Delegate.onInstV128Not(); break;
    case 0x4e: Delegate.onInstV128And(); break;
    case 0x4f: Delegate.onInstV128AndNot(); break;
    case 0x50: Delegate.onInstV128Or(); break;
    case 0x51: Delegate.onInstV128Xor(); break;
    case 0x52: Delegate.onInstV128BitSelect(); break;
    case 0x60: Delegate.onInstI8x16Abs(); break;
    case 0x61: Delegate.onInstI8x16Neg(); break;
    case 0x63: Delegate.onInstI8x16AllTrue(); break;
    case 0x64: Delegate.onInstI8x16Bitmask(); break;
    case 0x65: Delegate.onInstI8x16NarrowI16x8S(); break;
    case 0x66: Delegate.onInstI8x16NarrowI16x8U(); break;
    case 0x6b: Delegate.onInstI8x16Shl(); break;
    case 0x6c: Delegate.onInstI8x16ShrS(); break;
    case 0x6d: Delegate.onInstI8x16ShrU(); break;
    case 0x6e: Delegate.onInstI8x16Add(); break;
    case 0x6f: Delegate.onInstI8x16AddSatS(); break;
    case 0x70: Delegate.onInstI8x16AddSatU(); break;
    case 0x71: Delegate.onInstI8x16Sub(); break;
    case 0x72: Delegate.onInstI8x16SubSatS(); break;
    case 0x73: Delegate.onInstI8x16SubSatU(); break;
    case 0x76: Delegate.onInstI8x16MinS(); break;
    case 0x77: Delegate.onInstI8x16MinU(); break;
    case 0x78: Delegate.onInstI8x16MaxS(); break;
    case 0x79: Delegate.onInstI8x16MaxU(); break;
    case 0x7b: Delegate.onInstI8x16AvgrU(); break;
    case 0x80: Delegate.onInstI16x8Abs(); break;
    case 0x81: Delegate.onInstI16x8Neg(); break;
    case 0x83: Delegate.onInstI16x8AllTrue(); break;
    case 0x84: Delegate.onInstI16x8Bitmask(); break;
    case 0x85: Delegate.onInstI16x8NarrowI32x4S(); break;
    case 0x86: Delegate.onInstI16x8NarrowI32x4U(); break;
    case 0x87: Delegate.onInstI16x8ExtendLowI8x16S(); break;
    case 0x88: Delegate.onInstI16x8ExtendHighI8x16S(); break;
    case 0x89: Delegate.onInstI16x8ExtendLowI8x16U(); break;
    case 0x8a: Delegate.onInstI16x8ExtendHighI8x16U(); break;
    case 0x8b: Delegate.onInstI16x8Shl(); break;
    case 0x8c: Delegate.onInstI16x8ShrS(); break;
    case 0x8d: Delegate.onInstI16x8ShrU(); break;
    case 0x8e: Delegate.onInstI16x8Add(); break;
    case 0x8f: Delegate.onInstI16x8AddSatS(); break;
    case 0x90: Delegate.onInstI16x8AddSatU(); break;
    case 0x91: Delegate.onInstI16x8Sub(); break;
    case 0x92: Delegate.onInstI16x8SubSatS(); break;
    case 0x93: Delegate.onInstI16x8SubSatU(); break;
    case 0x95: Delegate.onInstI16x8Mul(); break;
    case 0x96: Delegate.onInstI16x8MinS(); break;
    case 0x97: Delegate.onInstI16x8MinU(); break;
    case 0x98: Delegate.onInstI16x8MaxS(); break;
    case 0x99: Delegate.onInstI16x8MaxU(); break;
    case 0x9b: Delegate.onInstI16x8AvgrU(); break;
    case 0xa0: Delegate.onInstI32x4Abs(); break;
    case 0xa1: Delegate.onInstI32x4Neg(); break;
    case 0xa3: Delegate.onInstI32x4AllTrue(); break;
    case 0xa4: Delegate.onInstI32x4Bitmask(); break;
    case 0xa7: Delegate.onInstI32x4ExtendLowI16x8S(); break;
    case 0xa8: Delegate.onInstI32x4ExtendHighI16x8S(); break;
    case 0xa9: Delegate.onInstI32x4ExtendLowI16x8U(); break;
    case 0xaa: Delegate.onInstI32x4ExtendHighI16x8U(); break;
    case 0xab: Delegate.onInstI32x4Shl(); break;
    case 0xac: Delegate.onInstI32x4ShrS(); break;
    case 0xad: Delegate.onInstI32x4ShrU(); break;
    case 0xae: Delegate.onInstI32x4Add(); break;
    case 0xb1: Delegate.onInstI32x4Sub(); break;
    case 0xb5: Delegate.onInstI32x4Mul(); break;
    case 0xb6: Delegate.onInstI32x4MinS(); break;
    case 0xb7: Delegate.onInstI32x4MinU(); break;
    case 0xb8: Delegate.onInstI32x4MaxS(); break;
    case 0xb9: Delegate.onInstI32x4MaxU(); break;
    case 0xba: Delegate.onInstI32x4DotI16x8S(); break;
    case 0xc0: Delegate.onInstI64x2Abs(); break;
    case 0xc1: Delegate.onInstI64x2Neg(); break;
    case 0xc4: Delegate.onInstI64x2Bitmask(); break;
    case 0xc7: Delegate.onInstI64x2ExtendLowI32x4S(); break;
    case 0xc8: Delegate.onInstI64x2ExtendHighI32x4S(); break;
    case 0xc9: Delegate.onInstI64x2ExtendLowI32x4U(); break;
    case 0xca: Delegate.onInstI64x2ExtendHighI32x4U(); break;
    case 0xcb: Delegate.onInstI64x2Shl(); break;
    case 0xcc: Delegate.onInstI64x2ShrS(); break;
    case 0xcd: Delegate.onInstI64x2ShrU(); break;
    case 0xce: Delegate.onInstI64x2Add(); break;
    case 0xd1: Delegate.onInstI64x2Sub(); break;
    case 0xd5: Delegate.onInstI64x2Mul(); break;
    case 0x67: Delegate.onInstF32x4Ceil(); break;
    case 0x68: Delegate.onInstF32x4Floor(); break;
    case 0x69: Delegate.onInstF32x4Trunc(); break;
    case 0x6a: Delegate.onInstF32x4Nearest(); break;
    case 0x74: Delegate.onInstF64x2Ceil(); break;
    case 0x75: Delegate.onInstF64x2Floor(); break;
    case 0x7a: Delegate.onInstF64x2Trunc(); break;
    case 0x94: Delegate.onInstF64x2Nearest(); break;
    case 0xe0: Delegate.onInstF32x4Abs(); break;
    case 0xe3: Delegate.onInstF32x4Neg(); break;
    case 0xe4: Delegate.onInstF32x4Sqrt(); break;
    case 0xe5: Delegate.onInstF32x4Add(); break;
    case 0xe6: Delegate.onInstF32x4Mul(); break;
    case 0xe7: Delegate.onInstF32x4Div(); break;
    case 0xe8: Delegate.onInstF32x4Min(); break;
    case 0xe9: Delegate.onInstF32x4Max(); break;
    case 0xea: Delegate.onInstF32x4PMin(); break;
    case 0xeb: Delegate.onInstF32x4PMax(); break;
    case 0xec: Delegate.onInstF64x2Abs(); break;
    case 0xed: Delegate.onInstF64x2Neg(); break;
    case 0xef: Delegate.onInstF64x2Sqrt(); break;
    case 0xf0: Delegate.onInstF64x2Add(); break;
    case 0xf1: Delegate.onInstF64x2Sub(); break;
    case 0xf2: Delegate.onInstF64x2Mul(); break;
    case 0xf3: Delegate.onInstF64x2Div(); break;
    case 0xf4: Delegate.onInstF64x2Min(); break;
    case 0xf5: Delegate.onInstF64x2Max(); break;
    case 0xf6: Delegate.onInstF64x2PMin(); break;
    case 0xf7: Delegate.onInstF64x2PMax(); break;
    case 0xf8: Delegate.onInstI32x4TruncSatF32x4S(); break;
    case 0xf9: Delegate.onInstI32x4TruncSatF32x4U(); break;
    case 0xfa: Delegate.onInstF32x4ConvertI32x4S(); break;
    case 0xfb: Delegate.onInstF32x4ConvertI32x4U(); break;
    case 0x5c: {
      auto Align = Reader.readULEB128Int32();
      auto Offset = Reader.readULEB128Int32();
      Delegate.onInstV128Load32Zero(Align, Offset);
      break;
    }
    case 0x5d: {
      auto Align = Reader.readULEB128Int32();
      auto Offset = Reader.readULEB128Int32();
      Delegate.onInstV128Load64Zero(Align, Offset);
      break;
    }
    case 0x9c: Delegate.onInstI16x8ExtMulLowI8x16S(); break;
    case 0x9d: Delegate.onInstI16x8ExtMulHighI8x16S(); break;
    case 0x9e: Delegate.onInstI16x8ExtMulLowI8x16U(); break;
    case 0x9f: Delegate.onInstI16x8ExtMulHighI8x16U(); break;
    case 0xbc: Delegate.onInstI32x4ExtMulLowI16x8S(); break;
    case 0xbd: Delegate.onInstI32x4ExtMulHighI16x8S(); break;
    case 0xbe: Delegate.onInstI32x4ExtMulLowI16x8U(); break;
    case 0xbf: Delegate.onInstI32x4ExtMulHighI16x8U(); break;
    case 0xdc: Delegate.onInstI64x2ExtMulLowI32x4S(); break;
    case 0xdd: Delegate.onInstI64x2ExtMulHighI32x4S(); break;
    case 0xde: Delegate.onInstI64x2ExtMulLowI32x4U(); break;
    case 0xdf: Delegate.onInstI64x2ExtMulHighI32x4U(); break;
    case 0x82: Delegate.onInstI16x8Q15MulRSatS(); break;
    case 0x53: Delegate.onInstV128AnyTrue(); break;
#define SABLE_LOAD_STORE_LANE_INSTRUCTION(Opcode, Name)                        \
  case Opcode: {                                                               \
    auto Align = Reader.readULEB128Int32();                                    \
    auto Offset = Reader.readULEB128Int32();                                   \
    auto LaneIndex = static_cast<bytecode::SIMDLaneID>(Reader.read());         \
    Delegate.onInst##Name(Align, Offset, LaneIndex);                           \
    break;                                                                     \
  }
      SABLE_LOAD_STORE_LANE_INSTRUCTION(0x54, V128Load8Lane)
      SABLE_LOAD_STORE_LANE_INSTRUCTION(0x55, V128Load16Lane)
      SABLE_LOAD_STORE_LANE_INSTRUCTION(0x56, V128Load32Lane)
      SABLE_LOAD_STORE_LANE_INSTRUCTION(0x57, V128Load64Lane)
      SABLE_LOAD_STORE_LANE_INSTRUCTION(0x58, V128Store8Lane)
      SABLE_LOAD_STORE_LANE_INSTRUCTION(0x59, V128Store16Lane)
      SABLE_LOAD_STORE_LANE_INSTRUCTION(0x5a, V128Store32Lane)
      SABLE_LOAD_STORE_LANE_INSTRUCTION(0x5b, V128Store64Lane)
#undef SABLE_LOAD_STORE_LANE_INSTRUCTION
    case 0xd6: Delegate.onInstI64x2Eq(); break;
    case 0xd7: Delegate.onInstI64x2Ne(); break;
    case 0xd8: Delegate.onInstI64x2LtS(); break;
    case 0xd9: Delegate.onInstI64x2GtS(); break;
    case 0xda: Delegate.onInstI64x2LeS(); break;
    case 0xdb: Delegate.onInstI64x2GeS(); break;
    case 0xc3: Delegate.onInstI64x2AllTrue(); break;
    case 0xfe: Delegate.onInstF64x2ConvertLowI32x4S(); break;
    case 0xff: Delegate.onInstF64x2ConvertLowI32x4U(); break;
    case 0xfc: Delegate.onInstI32x4TruncSatF64x2SZero(); break;
    case 0xfd: Delegate.onInstI32x4TruncSatF64x2UZero(); break;
    case 0x5e: Delegate.onInstF32x4DemoteF64x2Zero(); break;
    case 0x5f: Delegate.onInstF64x2PromoteLowF32x4(); break;
    case 0x62: Delegate.onInstI8x16Popcnt(); break;
    case 0x7c: Delegate.onInstI16x8ExtAddPairwiseI8x16S(); break;
    case 0x7d: Delegate.onInstI16x8ExtAddPairwiseI8x16U(); break;
    case 0x7e: Delegate.onInstI32x4ExtAddPairwiseI16x8S(); break;
    case 0x7f: Delegate.onInstI32x4ExtAddPairwiseI16x8U(); break;
    default:
      throw ParserError(
          fmt::format("unknown simd instruction 0xfd 0x{:02x}", SIMDOpcode));
    }
  }
}; // namespace parser
} // namespace parser

#endif