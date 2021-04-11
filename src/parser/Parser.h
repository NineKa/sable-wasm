#ifndef SABLE_INCLUDE_GUARD_PARSER_PARSER
#define SABLE_INCLUDE_GUARD_PARSER_PARSER

#include "../utility/Commons.h"
#include "Delegate.h"
#include "Reader.h"
#include "SIMD128Parser.tcc"
#include "SaturationArithmeticParser.tcc"

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/fill_n.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/reverse.hpp>
#include <range/v3/core.hpp>
#include <range/v3/iterator.hpp>
#include <range/v3/view/reverse.hpp>

#include <array>
#include <vector>

namespace parser {
class CustomSection {
  char const *NameTag;

public:
  explicit CustomSection(char const *NameTag_) : NameTag(NameTag_) {}
  CustomSection(CustomSection const &) = default;
  CustomSection(CustomSection &&) noexcept = default;
  CustomSection &operator=(CustomSection const &) = default;
  CustomSection &operator=(CustomSection &&) noexcept = default;
  virtual ~CustomSection() noexcept = default;

  char const *getNameTag() const { return NameTag; }
  virtual void parse(std::span<std::byte const> Bytes) = 0;
};

class CustomSectionError : public std::runtime_error {
  std::size_t Offset;

public:
  CustomSectionError(std::size_t Offset_, char const *Msg)
      : std::runtime_error(Msg), Offset(Offset_) {}
  CustomSectionError(std::size_t Offset_, std::string const &Msg)
      : std::runtime_error(Msg), Offset(Offset_) {}
  std::size_t getSiteOffset() const { return Offset; }
};

template <reader ReaderImpl, delegate DelegateImpl> class Parser {
  WASMReader<ReaderImpl> Reader;
  DelegateImpl &Delegate;
  std::vector<CustomSection *> CustomSections;

  std::byte parseInstructions(std::span<std::byte const> const &);

public:
  Parser(ReaderImpl &Reader_, DelegateImpl &Delegate_)
      : Reader(Reader_), Delegate(Delegate_) {}

  void registerCustomSection(CustomSection &Handler) {
    CustomSections.push_back(std::addressof(Handler));
  }

  void parseExpression();
  void parseInstruction();

  void validateMagicNumber();
  void validateVersion();

  void parseTypeSection();
  void parseImportSection();
  void parseFunctionSection();
  void parseTableSection();
  void parseMemorySection();
  void parseGlobalSection();
  void parseExportSection();
  void parseStartSection();
  void parseElementSection();
  void parseCodeSection();
  void parseDataSection();

  void parseCustomSection(std::size_t Size);

  void parse();
};

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::validateMagicNumber() {
  using namespace utility::literals;
  std::array ExpectMagicNumber{0x00_byte, 0x61_byte, 0x73_byte, 0x6d_byte};
  std::array<std::byte, 4> MagicNumber{};
  ranges::copy(Reader.read(4), ranges::begin(MagicNumber));
  if (ExpectMagicNumber != MagicNumber)
    throw ParserError(fmt::format(
        "unknown magic number 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x}, expecting "
        "0x00 0x61 0x73 0x6d",
        MagicNumber[0], MagicNumber[1], MagicNumber[2], MagicNumber[3]));
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::validateVersion() {
  using namespace utility::literals;
  std::array ExpectVersion{0x01_byte, 0x00_byte, 0x00_byte, 0x00_byte};
  std::array<std::byte, 4> Version{};
  ranges::copy(Reader.read(4), ranges::begin(Version));
  if (ExpectVersion != Version)
    throw ParserError(fmt::format(
        "unknown version 0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x}, expecting 0x01 "
        "0x00 0x00 0x00",
        Version[0], Version[1], Version[2], Version[3]));
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseTypeSection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterTypeSection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto Type = Reader.readFunctionType();
    Delegate.onTypeSectionEntry(I, std::move(Type));
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseImportSection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterImportSection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto ModuleName = Reader.readUTF8StringVector();
    auto EntityName = Reader.readUTF8StringVector();
    auto Descriptor = Reader.readImportDescriptor();
    Delegate.onImportSectionEntry(I, ModuleName, EntityName, Descriptor);
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseFunctionSection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterFunctionSection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto TypeIndex = Reader.readTypeIDX();
    Delegate.onFunctionSectionEntry(I, TypeIndex);
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseTableSection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterTableSection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto Type = Reader.readTableType();
    Delegate.onTableSectionEntry(I, Type);
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseMemorySection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterMemorySection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto Type = Reader.readMemoryType();
    Delegate.onMemorySectionEntry(I, Type);
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseGlobalSection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterGlobalSection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto Type = Reader.readGlobalType();
    parseExpression();
    Delegate.onGlobalSectionEntry(I, Type);
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseExportSection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterExportSection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto EntityName = Reader.readUTF8StringVector();
    auto ExportDescriptor = Reader.readExportDescriptor();
    Delegate.onExportSectionEntry(I, EntityName, ExportDescriptor);
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseStartSection() {
  auto StartFunc = Reader.readFuncIDX();
  Delegate.onStartSectionEntry(StartFunc);
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseElementSection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterElementSection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto TableIndex = Reader.readTableIDX();
    parseExpression();
    auto NumFuncIndices = Reader.readULEB128Int32();
    std::vector<bytecode::FuncIDX> FuncIndices;
    FuncIndices.reserve(NumFuncIndices);
    for (decltype(NumFuncIndices) J = 0; J < NumFuncIndices; ++J) {
      auto FuncIndex = Reader.readFuncIDX();
      FuncIndices.push_back(FuncIndex);
    }
    Delegate.onElementSectionEntry(I, TableIndex, std::move(FuncIndices));
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseCodeSection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterCodeSection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto EnteringBarrierStatus = Reader.backupBarrier();
    auto PayloadSize = Reader.readULEB128Int32();
    Reader.setBarrier(PayloadSize);
    auto NumLocalEntries = Reader.readULEB128Int32();
    std::vector<bytecode::ValueType> LocalTypes;
    for (decltype(NumLocalEntries) J = 0; J < NumLocalEntries; ++J) {
      auto N = Reader.readULEB128Int32();
      auto Type = Reader.readValueType();
      ranges::fill_n(ranges::back_inserter(LocalTypes), N, Type);
    }
    Delegate.onCodeSectionLocal(I, std::move(LocalTypes));
    parseExpression();
    Delegate.onCodeSectionEntry(I);
    if (Reader.hasMoreBytes())
      throw ParserError("code section entry has unconsumed bytes");
    Reader.restoreBarrier(EnteringBarrierStatus);
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseDataSection() {
  auto NumEntries = Reader.readULEB128Int32();
  Delegate.enterDataSection(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto MemoryIndex = Reader.readMemIDX();
    parseExpression();
    auto NumBytes = Reader.readULEB128Int32();
    auto Bytes = Reader.read(NumBytes);
    Delegate.onDataSectionEntry(I, MemoryIndex, Bytes);
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseCustomSection(std::size_t Size) {
  auto EnterNumBytesConsumed = Reader.getNumBytesConsumed();
  auto NameTag = Reader.readUTF8StringVector();
  auto ExitNumBytesConsumed = Reader.getNumBytesConsumed();
  auto PayloadSize = Size - (ExitNumBytesConsumed - EnterNumBytesConsumed);
  auto PayloadStartCursorStatus = Reader.backupCursor();
  auto Payload = Reader.read(PayloadSize);
  for (auto const &CustomSection : CustomSections) {
    if (NameTag == CustomSection->getNameTag()) {
      try {
        CustomSection->parse(Payload);
      } catch (CustomSectionError const &E) {
        Reader.restoreCursor(PayloadStartCursorStatus);
        Reader.skip(E.getSiteOffset());
        throw ParserError(E.what());
      }
    }
  }
}

namespace detail {
struct WASMSectionOrderPolicy {
  unsigned PreviousSectionNumber = 0;
  void check(std::byte SectionMagicNumber) {
    using namespace utility::literals;
    /* custom section can appear any where */
    if (SectionMagicNumber == 0x00_byte) return;
    if (static_cast<unsigned>(SectionMagicNumber) <= PreviousSectionNumber)
      throw ParserError("invalid section ordering");
    PreviousSectionNumber = static_cast<unsigned>(SectionMagicNumber);
  }
};
} // namespace detail

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parse() {
  validateMagicNumber();
  validateVersion();
  detail::WASMSectionOrderPolicy OrderPolicy;
  while (Reader.hasMoreBytes()) {
    auto SectionMagicNumber = Reader.read();
    auto SectionSize = Reader.readULEB128Int32();
    Reader.setBarrier(SectionSize);
    OrderPolicy.check(SectionMagicNumber);
    switch (static_cast<unsigned>(SectionMagicNumber)) {
    case 0x00: parseCustomSection(SectionSize); break;
    case 0x01: parseTypeSection(); break;
    case 0x02: parseImportSection(); break;
    case 0x03: parseFunctionSection(); break;
    case 0x04: parseTableSection(); break;
    case 0x05: parseMemorySection(); break;
    case 0x06: parseGlobalSection(); break;
    case 0x07: parseExportSection(); break;
    case 0x08: parseStartSection(); break;
    case 0x09: parseElementSection(); break;
    case 0x0a: parseCodeSection(); break;
    case 0x0b: parseDataSection(); break;
    default:
      throw ParserError(fmt::format(
          "unknown section magic number 0x{:02x}", SectionMagicNumber));
    }
    if (Reader.hasMoreBytes())
      throw ParserError("section has unconsumed bytes");
    Reader.resetBarrier();
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseExpression() {
  using namespace utility::literals;
  Delegate.enterExpression();
  std::array TerminatingByte{0x0b_byte};
  parseInstructions(TerminatingByte);
  Delegate.exitExpression();
}

template <reader ReaderImpl, delegate DelegateImpl>
std::byte Parser<ReaderImpl, DelegateImpl>::parseInstructions(
    std::span<std::byte const> const &TerminatingBytes) {
  using namespace utility::literals;
  auto PeekByte = Reader.peek();
  auto EndSentinel = ranges::end(TerminatingBytes);
  while (ranges::find(TerminatingBytes, PeekByte) == EndSentinel) {
    parseInstruction();
    PeekByte = Reader.peek();
  }
  return Reader.read();
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parseInstruction() {
  using namespace utility::literals;
  auto OpcodeByte = Reader.read();
  switch (static_cast<unsigned>(OpcodeByte)) {
  case 0x00: Delegate.onInstUnreachable(); break;
  case 0x01: Delegate.onInstNop(); break;
  case 0x02: {
    Delegate.enterInstBlock(Reader.readBlockResultType());
    std::array TerminatingByte{0x0b_byte};
    parseInstructions(TerminatingByte);
    Delegate.exitInstBlock();
    break;
  }
  case 0x03: {
    Delegate.enterInstLoop(Reader.readBlockResultType());
    std::array TerminatingByte{0x0b_byte};
    parseInstructions(TerminatingByte);
    Delegate.exitInstLoop();
    break;
  }
  case 0x04: {
    Delegate.enterInstIf(Reader.readBlockResultType());
    std::array TerminatingBytes{0x05_byte, 0x0b_byte};
    if (parseInstructions(TerminatingBytes) == 0x05_byte) {
      Delegate.enterInstElse();
      std::array TerminatingByte{0x0b_byte};
      parseInstructions(TerminatingByte);
    }
    Delegate.exitInstIf();
    break;
  }
  case 0x0c: Delegate.onInstBr(Reader.readLabelIDX()); break;
  case 0x0d: Delegate.onInstBrIf(Reader.readLabelIDX()); break;
  case 0x0e: {
    auto NumTargetEntries = Reader.readULEB128Int32();
    std::vector<bytecode::LabelIDX> Targets;
    Targets.reserve(NumTargetEntries);
    for (decltype(NumTargetEntries) I = 0; I < NumTargetEntries; ++I) {
      auto Target = Reader.readLabelIDX();
      Targets.push_back(Target);
    }
    auto DefaultTarget = Reader.readLabelIDX();
    Delegate.onInstBrTable(DefaultTarget, Targets);
    break;
  }
  case 0x0f: Delegate.onInstReturn(); break;
  case 0x10: Delegate.onInstCall(Reader.readFuncIDX()); break;
  case 0x11:
    Delegate.onInstCallIndirect(Reader.readTypeIDX());
    utility::ignore(Reader.read());
    break;

  case 0x1a: Delegate.onInstDrop(); break;
  case 0x1b: Delegate.onInstSelect(); break;

#define SABLE_LOCAL_INSTRUCTION(Opcode, Name)                                  \
  case Opcode: Delegate.onInst##Name(Reader.readLocalIDX()); break;
    SABLE_LOCAL_INSTRUCTION(0x20, LocalGet)
    SABLE_LOCAL_INSTRUCTION(0x21, LocalSet)
    SABLE_LOCAL_INSTRUCTION(0x22, LocalTee)
#undef SABLE_LOCAL_INSTRUCTION
#define SABLE_GLOBAL_INSTRUCTION(Opcode, Name)                                 \
  case Opcode: Delegate.onInst##Name(Reader.readGlobalIDX()); break;
    SABLE_GLOBAL_INSTRUCTION(0x23, GlobalGet)
    SABLE_GLOBAL_INSTRUCTION(0x24, GlobalSet)
#undef SABLE_GLOBAL_INSTRUCTION

#define SABLE_MEMORY_INSTRUCTION(Opcode, Name)                                 \
  case Opcode: {                                                               \
    auto Align = Reader.readULEB128Int32();                                    \
    auto Offset = Reader.readULEB128Int32();                                   \
    Delegate.onInst##Name(Align, Offset);                                      \
    break;                                                                     \
  }
    SABLE_MEMORY_INSTRUCTION(0x28, I32Load)
    SABLE_MEMORY_INSTRUCTION(0x29, I64Load)
    SABLE_MEMORY_INSTRUCTION(0x2a, F32Load)
    SABLE_MEMORY_INSTRUCTION(0x2b, F64Load)
    SABLE_MEMORY_INSTRUCTION(0x2c, I32Load8S)
    SABLE_MEMORY_INSTRUCTION(0x2d, I32Load8U)
    SABLE_MEMORY_INSTRUCTION(0x2e, I32Load16S)
    SABLE_MEMORY_INSTRUCTION(0x2f, I32Load16U)
    SABLE_MEMORY_INSTRUCTION(0x30, I64Load8S)
    SABLE_MEMORY_INSTRUCTION(0x31, I64Load8U)
    SABLE_MEMORY_INSTRUCTION(0x32, I64Load16S)
    SABLE_MEMORY_INSTRUCTION(0x33, I64Load16U)
    SABLE_MEMORY_INSTRUCTION(0x34, I64Load32S)
    SABLE_MEMORY_INSTRUCTION(0x35, I64Load32U)
    SABLE_MEMORY_INSTRUCTION(0x36, I32Store)
    SABLE_MEMORY_INSTRUCTION(0x37, I64Store)
    SABLE_MEMORY_INSTRUCTION(0x38, F32Store)
    SABLE_MEMORY_INSTRUCTION(0x39, F64Store)
    SABLE_MEMORY_INSTRUCTION(0x3a, I32Store8)
    SABLE_MEMORY_INSTRUCTION(0x3b, I32Store16)
    SABLE_MEMORY_INSTRUCTION(0x3c, I64Store8)
    SABLE_MEMORY_INSTRUCTION(0x3d, I64Store16)
    SABLE_MEMORY_INSTRUCTION(0x3e, I64Store32)
#undef SABLE_MEMORY_INSTRUCTION
  case 0x3f: {
    utility::ignore(Reader.read());
    Delegate.onInstMemorySize();
    break;
  }
  case 0x40: {
    utility::ignore(Reader.read());
    Delegate.onInstMemoryGrow();
    break;
  }

  case 0x41: Delegate.onInstI32Const(Reader.readSLEB128Int32()); break;
  case 0x42: Delegate.onInstI64Const(Reader.readSLEB128Int64()); break;
  case 0x43: {
    auto Bytes = Reader.read(4);
    std::array<float, 1> Result = {0x0f};
    auto ResultByteView = std::as_writable_bytes(std::span{Result});
    assert(Bytes.size() == ResultByteView.size());
    if constexpr (std::endian::native == std::endian::little) {
      ranges::copy(Bytes, ranges::begin(ResultByteView));
    } else if constexpr (std::endian::native == std::endian::big) {
      auto ReverseView = ranges::views::reverse(Bytes);
      ranges::copy(ReverseView, ranges::begin(ResultByteView));
    } else {
      utility::unreachable();
    }
    Delegate.onInstF32Const(Result[0]);
    break;
  }
  case 0x44: {
    auto Bytes = Reader.read(8);
    std::array<double, 1> Result = {0x0};
    auto ResultByteView = std::as_writable_bytes(std::span{Result});
    assert(Bytes.size() == ResultByteView.size());
    if constexpr (std::endian::native == std::endian::little) {
      ranges::copy(Bytes, ranges::begin(ResultByteView));
    } else if constexpr (std::endian::native == std::endian::big) {
      auto ReverseView = ranges::views::reverse(Bytes);
      ranges::copy(ReverseView, ranges::begin(ResultByteView));
    } else {
      utility::unreachable();
    }
    Delegate.onInstF64Const(Result[0]);
    break;
  }

  case 0x45: Delegate.onInstI32Eqz(); break;
  case 0x46: Delegate.onInstI32Eq(); break;
  case 0x47: Delegate.onInstI32Ne(); break;
  case 0x48: Delegate.onInstI32LtS(); break;
  case 0x49: Delegate.onInstI32LtU(); break;
  case 0x4a: Delegate.onInstI32GtS(); break;
  case 0x4b: Delegate.onInstI32GtU(); break;
  case 0x4c: Delegate.onInstI32LeS(); break;
  case 0x4d: Delegate.onInstI32LeU(); break;
  case 0x4e: Delegate.onInstI32GeS(); break;
  case 0x4f: Delegate.onInstI32GeU(); break;

  case 0x50: Delegate.onInstI64Eqz(); break;
  case 0x51: Delegate.onInstI64Eq(); break;
  case 0x52: Delegate.onInstI64Ne(); break;
  case 0x53: Delegate.onInstI64LtS(); break;
  case 0x54: Delegate.onInstI64LtU(); break;
  case 0x55: Delegate.onInstI64GtS(); break;
  case 0x56: Delegate.onInstI64GtU(); break;
  case 0x57: Delegate.onInstI64LeS(); break;
  case 0x58: Delegate.onInstI64LeU(); break;
  case 0x59: Delegate.onInstI64GeS(); break;
  case 0x5a: Delegate.onInstI64GeU(); break;

  case 0x5b: Delegate.onInstF32Eq(); break;
  case 0x5c: Delegate.onInstF32Ne(); break;
  case 0x5d: Delegate.onInstF32Lt(); break;
  case 0x5e: Delegate.onInstF32Gt(); break;
  case 0x5f: Delegate.onInstF32Le(); break;
  case 0x60: Delegate.onInstF32Ge(); break;

  case 0x61: Delegate.onInstF64Eq(); break;
  case 0x62: Delegate.onInstF64Ne(); break;
  case 0x63: Delegate.onInstF64Lt(); break;
  case 0x64: Delegate.onInstF64Gt(); break;
  case 0x65: Delegate.onInstF64Le(); break;
  case 0x66: Delegate.onInstF64Ge(); break;

  case 0x67: Delegate.onInstI32Clz(); break;
  case 0x68: Delegate.onInstI32Ctz(); break;
  case 0x69: Delegate.onInstI32Popcnt(); break;
  case 0x6a: Delegate.onInstI32Add(); break;
  case 0x6b: Delegate.onInstI32Sub(); break;
  case 0x6c: Delegate.onInstI32Mul(); break;
  case 0x6d: Delegate.onInstI32DivS(); break;
  case 0x6e: Delegate.onInstI32DivU(); break;
  case 0x6f: Delegate.onInstI32RemS(); break;
  case 0x70: Delegate.onInstI32RemU(); break;
  case 0x71: Delegate.onInstI32And(); break;
  case 0x72: Delegate.onInstI32Or(); break;
  case 0x73: Delegate.onInstI32Xor(); break;
  case 0x74: Delegate.onInstI32Shl(); break;
  case 0x75: Delegate.onInstI32ShrS(); break;
  case 0x76: Delegate.onInstI32ShrU(); break;
  case 0x77: Delegate.onInstI32Rotl(); break;
  case 0x78: Delegate.onInstI32Rotr(); break;

  case 0x79: Delegate.onInstI64Clz(); break;
  case 0x7a: Delegate.onInstI64Ctz(); break;
  case 0x7b: Delegate.onInstI64Popcnt(); break;
  case 0x7c: Delegate.onInstI64Add(); break;
  case 0x7d: Delegate.onInstI64Sub(); break;
  case 0x7e: Delegate.onInstI64Mul(); break;
  case 0x7f: Delegate.onInstI64DivS(); break;
  case 0x80: Delegate.onInstI64DivU(); break;
  case 0x81: Delegate.onInstI64RemS(); break;
  case 0x82: Delegate.onInstI64RemU(); break;
  case 0x83: Delegate.onInstI64And(); break;
  case 0x84: Delegate.onInstI64Or(); break;
  case 0x85: Delegate.onInstI64Xor(); break;
  case 0x86: Delegate.onInstI64Shl(); break;
  case 0x87: Delegate.onInstI64ShrS(); break;
  case 0x88: Delegate.onInstI64ShrU(); break;
  case 0x89: Delegate.onInstI64Rotl(); break;
  case 0x8a: Delegate.onInstI64Rotr(); break;

  case 0x8b: Delegate.onInstF32Abs(); break;
  case 0x8c: Delegate.onInstF32Neg(); break;
  case 0x8d: Delegate.onInstF32Ceil(); break;
  case 0x8e: Delegate.onInstF32Floor(); break;
  case 0x8f: Delegate.onInstF32Trunc(); break;
  case 0x90: Delegate.onInstF32Nearest(); break;
  case 0x91: Delegate.onInstF32Sqrt(); break;
  case 0x92: Delegate.onInstF32Add(); break;
  case 0x93: Delegate.onInstF32Sub(); break;
  case 0x94: Delegate.onInstF32Mul(); break;
  case 0x95: Delegate.onInstF32Div(); break;
  case 0x96: Delegate.onInstF32Min(); break;
  case 0x97: Delegate.onInstF32Max(); break;
  case 0x98: Delegate.onInstF32CopySign(); break;

  case 0x99: Delegate.onInstF64Abs(); break;
  case 0x9a: Delegate.onInstF64Neg(); break;
  case 0x9b: Delegate.onInstF64Ceil(); break;
  case 0x9c: Delegate.onInstF64Floor(); break;
  case 0x9d: Delegate.onInstF64Trunc(); break;
  case 0x9e: Delegate.onInstF64Nearest(); break;
  case 0x9f: Delegate.onInstF64Sqrt(); break;
  case 0xa0: Delegate.onInstF64Add(); break;
  case 0xa1: Delegate.onInstF64Sub(); break;
  case 0xa2: Delegate.onInstF64Mul(); break;
  case 0xa3: Delegate.onInstF64Div(); break;
  case 0xa4: Delegate.onInstF64Min(); break;
  case 0xa5: Delegate.onInstF64Max(); break;
  case 0xa6: Delegate.onInstF64CopySign(); break;

  case 0xa7: Delegate.onInstI32WrapI64(); break;
  case 0xa8: Delegate.onInstI32TruncF32S(); break;
  case 0xa9: Delegate.onInstI32TruncF32U(); break;
  case 0xaa: Delegate.onInstI32TruncF64S(); break;
  case 0xab: Delegate.onInstI32TruncF64U(); break;
  case 0xac: Delegate.onInstI64ExtendI32S(); break;
  case 0xad: Delegate.onInstI64ExtendI32U(); break;
  case 0xae: Delegate.onInstI64TruncF32S(); break;
  case 0xaf: Delegate.onInstI64TruncF32U(); break;
  case 0xb0: Delegate.onInstI64TruncF64S(); break;
  case 0xb1: Delegate.onInstI64TruncF64U(); break;
  case 0xb2: Delegate.onInstF32ConvertI32S(); break;
  case 0xb3: Delegate.onInstF32ConvertI32U(); break;
  case 0xb4: Delegate.onInstF32ConvertI64S(); break;
  case 0xb5: Delegate.onInstF32ConvertI64U(); break;
  case 0xb6: Delegate.onInstF32DemoteF64(); break;
  case 0xb7: Delegate.onInstF64ConvertI32S(); break;
  case 0xb8: Delegate.onInstF64ConvertI32U(); break;
  case 0xb9: Delegate.onInstF64ConvertI64S(); break;
  case 0xba: Delegate.onInstF64ConvertI64U(); break;
  case 0xbb: Delegate.onInstF64PromoteF32(); break;
  case 0xbc: Delegate.onInstI32ReinterpretF32(); break;
  case 0xbd: Delegate.onInstI64ReinterpretF64(); break;
  case 0xbe: Delegate.onInstF32ReinterpretI32(); break;
  case 0xbf: Delegate.onInstF64ReinterpretI64(); break;

  case 0xc0: Delegate.onInstI32Extend8S(); break;
  case 0xc1: Delegate.onInstI32Extend16S(); break;
  case 0xc2: Delegate.onInstI64Extend8S(); break;
  case 0xc3: Delegate.onInstI64Extend16S(); break;
  case 0xc4: Delegate.onInstI64Extend32S(); break;

  case static_cast<unsigned>(SaturationArithmeticParserBase::Prefix): {
    SaturationArithmeticParser ExtensionParser(Delegate);
    ExtensionParser(Reader);
    break;
  }

  case static_cast<unsigned>(SIMDParserBase::Prefix): {
    SIMDParser ExtensionParser(Delegate);
    ExtensionParser(Reader);
    break;
  }

  default:
    throw ParserError(
        fmt::format("unknown instruction opcode 0x{:02x}", OpcodeByte));
  }
}
} // namespace parser

#endif
