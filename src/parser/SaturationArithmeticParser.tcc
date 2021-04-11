#ifndef SABLE_INCLUDE_GUARD_PARSER_PARSER_SATURATION_ARITHMETIC
#define SABLE_INCLUDE_GUARD_PARSER_PARSER_SATURATION_ARITHMETIC

#include "Delegate.h"
#include "Reader.h"

namespace parser {
struct SaturationArithmeticParserBase {
  static constexpr std::byte Prefix = std::byte(0xfc);
};

template <delegate DelegateType>
struct SaturationArithmeticParser : SaturationArithmeticParserBase {
  DelegateType &Delegate;

  explicit SaturationArithmeticParser(DelegateType &Delegate_)
      : Delegate(Delegate_) {}

  template <reader ReaderBase> void operator()(WASMReader<ReaderBase> &Reader) {
    auto Opcode = Reader.read();
    switch (static_cast<unsigned>(Opcode)) {
    case 0x00: Delegate.onInstI32TruncSatF32S(); break;
    case 0x01: Delegate.onInstI32TruncSatF32U(); break;
    case 0x02: Delegate.onInstI32TruncSatF64S(); break;
    case 0x03: Delegate.onInstI32TruncSatF64U(); break;
    case 0x04: Delegate.onInstI64TruncSatF32S(); break;
    case 0x05: Delegate.onInstI64TruncSatF32U(); break;
    case 0x06: Delegate.onInstI64TruncSatF64S(); break;
    case 0x07: Delegate.onInstI64TruncSatF64U(); break;
    default:
      throw ParserError(fmt::format(
          "unknown saturation arithmetic instruction 0xfc 0x{:02x}", Opcode));
    }
  }
};
} // namespace parser

#endif