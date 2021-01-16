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
    auto FuncIndex = Reader.readFuncIDX();
    auto Name = Reader.readUTF8StringVector();
    FunctionNames.push_back(
        FunctionNameEntry{.FuncIndex = FuncIndex, .Name = std::string(Name)});
  }
  ranges::sort(FunctionNames);
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
    auto FuncIndex = Reader.readFuncIDX();
    auto NumSubEntries = Reader.readULEB128Int32();
    for (decltype(NumSubEntries) J = 0; J < NumSubEntries; ++J) {
      auto LocalIndex = Reader.readLocalIDX();
      auto Name = Reader.readUTF8StringVector();
      LocalNames.push_back(LocalNameEntry{
          .FuncIndex = FuncIndex,
          .LocalIndex = LocalIndex,
          .Name = std::string(Name)});
    }
  }
  ranges::sort(LocalNames);
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
  auto ProjectFn = [](FunctionNameEntry const &Pair) { return Pair.FuncIndex; };
  auto EqualRange = ranges::equal_range(FunctionNames, Func, {}, ProjectFn);
  if (ranges::empty(EqualRange)) return std::nullopt;
  return std::string_view(ranges::begin(EqualRange)->Name);
}

std::optional<std::string_view>
Name::getLocalName(bytecode::FuncIDX Func, bytecode::LocalIDX Local) const {
  auto SearchValue = std::make_pair(Func, Local);
  auto ProjectFn = [](LocalNameEntry const &Pair) {
    return std::make_pair(Pair.FuncIndex, Pair.LocalIndex);
  };
  auto EqualRange = ranges::equal_range(LocalNames, SearchValue, {}, ProjectFn);
  if (ranges::empty(EqualRange)) return std::nullopt;
  return std::string_view(ranges::begin(EqualRange)->Name);
}

} // namespace parser::customsections
