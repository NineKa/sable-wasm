#include "Name.h"

namespace parser::customsections {
void Name::parseModuleName(WASMReader<ByteArrayReader> &Reader) {
  using namespace utility::literals;
  if (!Reader.hasMoreBytes()) return;
  if (Reader.peek() != 0x00_byte) return;
  utility::ignore(Reader.read()); // consume the magic number
  auto EnteringBarrierStatus = Reader.backupBarrier();
  auto SubsectionSize = Reader.readULEB128Int32();
  Reader.setBarrier(SubsectionSize);
  ModuleName = Reader.readUTF8StringVector();
  Reader.restoreBarrier(EnteringBarrierStatus);
}

void Name::parseFunctionNames(WASMReader<ByteArrayReader> &Reader) {
  using namespace utility::literals;
  if (!Reader.hasMoreBytes()) return;
  if (Reader.peek() != 0x01_byte) return;
  utility::ignore(Reader.read()); // consume the magic number
  auto EnteringBarrierStatus = Reader.backupBarrier();
  auto SubsectionSize = Reader.readULEB128Int32();
  Reader.setBarrier(SubsectionSize);
  auto NumEntries = Reader.readULEB128Int32();
  FunctionNames.reserve(NumEntries);
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto FuncIndex = static_cast<bytecode::FuncIDX>(Reader.readULEB128Int32());
    auto Name = Reader.readUTF8StringVector();
    FunctionNames.emplace_back(FuncIndex, Name);
  }
  auto ProjectFn = [](auto &&Pair) { return std::get<0>(Pair); };
  ranges::sort(FunctionNames, std::less{}, ProjectFn);
  Reader.restoreBarrier(EnteringBarrierStatus);
}

void Name::parseLocalNames(WASMReader<ByteArrayReader> &Reader) {
  using namespace utility::literals;
  if (!Reader.hasMoreBytes()) return;
  if (Reader.peek() != 0x02_byte) return;
  utility::ignore(Reader.read()); // consume the magic number
  auto EnteringBarrierStatus = Reader.backupBarrier();
  auto SubsectionSize = Reader.readULEB128Int32();
  Reader.setBarrier(SubsectionSize);
  auto NumEntries = Reader.readULEB128Int32();
  for (decltype(NumEntries) I = 0; I < NumEntries; ++I) {
    auto FuncIndex = static_cast<bytecode::FuncIDX>(Reader.readULEB128Int32());
    auto NumSubEntries = Reader.readULEB128Int32();
    for (decltype(NumSubEntries) J = 0; J < NumSubEntries; ++J) {
      auto LocalIndex =
          static_cast<bytecode::LocalIDX>(Reader.readULEB128Int32());
      auto Name = Reader.readUTF8StringVector();
      LocalNames.emplace_back(FuncIndex, LocalIndex, Name);
    }
  }
  auto ProjectFn = [](auto &&Pair) {
    return std::make_pair(std::get<0>(Pair), std::get<1>(Pair));
  };
  ranges::sort(LocalNames, std::less{}, ProjectFn);
  Reader.restoreBarrier(EnteringBarrierStatus);
}

void Name::parse(std::span<std::byte const> Payload) {
  ByteArrayReader BaseReader(Payload);
  WASMReader<ByteArrayReader> Reader(BaseReader);
  try {
    parseModuleName(Reader);
    parseFunctionNames(Reader);
    parseLocalNames(Reader);
    if (Reader.hasMoreBytes())
      throw ParserError("custom section name has unconsumed bytes");
  } catch (ParserError const &E) {
    throw CustomSectionError(Reader.getNumBytesConsumed(), E.what());
  }
}

std::optional<std::string_view> Name::getModuleName() const {
  if (!ModuleName.has_value()) return std::nullopt;
  return std::string_view(*ModuleName);
}

std::optional<std::string_view>
Name::getFunctionName(bytecode::FuncIDX Func) const {
  auto ProjectFn = [](auto &&Pair) { return std::get<0>(Pair); };
  auto EqualRange = ranges::equal_range(FunctionNames, Func, {}, ProjectFn);
  if (ranges::empty(EqualRange)) return std::nullopt;
  return std::string_view(std::get<1>(*ranges::begin(EqualRange)));
}

std::optional<std::string_view>
Name::getLocalName(bytecode::FuncIDX Func, bytecode::LocalIDX Local) const {
  auto SearchValue = std::make_pair(Func, Local);
  auto ProjectFn = [](auto &&Pair) {
    return std::make_pair(std::get<0>(Pair), std::get<1>(Pair));
  };
  auto EqualRange = ranges::equal_range(LocalNames, SearchValue, {}, ProjectFn);
  if (ranges::empty(EqualRange)) return std::nullopt;
  return std::string_view(std::get<2>(*ranges::begin(EqualRange)));
}

} // namespace parser::customsections
