#ifndef SABLE_INCLUDE_GUARD_PARSER_PARSER
#define SABLE_INCLUDE_GUARD_PARSER_PARSER

#include "../utility/Commons.h"
#include "Delegate.h"
#include "Reader.h"

#include <array>
#include <ranges>

namespace parser {
template <reader ReaderImpl, delegate DelegateImpl> class Parser {
  WASMReader<ReaderImpl> Reader;
  DelegateImpl &Delegate;

public:
  Parser(ReaderImpl &Reader_, DelegateImpl &Delegate_)
      : Reader(Reader_), Delegate(Delegate_) {}

  void validateMagicNumber();
  void validateVersion();

  void parseTypeSection();
  void parseImportSection();

  void parse();
};

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::validateMagicNumber() {
  using namespace utility::literals;
  std::array ExpectMagicNumber{0x00_byte, 0x61_byte, 0x73_byte, 0x6d_byte};
  std::array<std::byte, 4> MagicNumber;
  std::ranges::copy(Reader.read(4), MagicNumber.begin());
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
  std::array<std::byte, 4> Version;
  std::ranges::copy(Reader.read(4), Version.begin());
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
  }
}

template <reader ReaderImpl, delegate DelegateImpl>
void Parser<ReaderImpl, DelegateImpl>::parse() {
  validateMagicNumber();
  validateVersion();
  while (Reader.hasMoreBytes()) {
    auto SectionMagicNumber = Reader.read();
    auto SectionSize = Reader.readULEB128Int32();
    Reader.setBarrier(SectionSize);
    switch (static_cast<unsigned>(SectionMagicNumber)) {
    case 0x01: parseTypeSection(); break;
    default: Reader.skip(SectionSize);
    }
    if (Reader.hasMoreBytes())
      throw ParserError("section has unconsumed bytes");
    Reader.resetBarrier();
  }
}
} // namespace parser

#endif