#ifndef SABLE_INCLUDE_GUARD_PARSER_READER
#define SABLE_INCLUDE_GUARD_PARSER_READER

#include "../bytecode/Instruction.h"
#include "../bytecode/Module.h"
#include "../bytecode/Type.h"
#include "../utility/Commons.h"

#include <fmt/format.h>
#include <utf8.h>

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

namespace parser {
struct ParserError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// clang-format off
template <typename T>
concept reader = requires(T R) {
  { R.read()                            } -> std::convertible_to<std::byte>;
  { R.read(std::declval<std::size_t>()) } 
  -> std::same_as<std::span<std::byte const>>;
  { R.peek()                            } -> std::convertible_to<std::byte>;
  { R.skip(std::declval<std::size_t>()) };
  { R.hasMoreBytes()                    } -> std::convertible_to<bool>;
  { R.getNumBytesConsumed()             } -> std::convertible_to<std::size_t>;
  typename T::CursorStatus;
  { R.backupCursor() } -> std::convertible_to<typename T::CursorStatus>;
  { R.restoreCursor(std::declval<typename T::CursorStatus>()) };
  typename T::BarrierStatus;
  { R.setBarrier(std::declval<std::size_t>()) };
  { R.resetBarrier()                          };
  { R.backupBarrier() } -> std::convertible_to<typename T::BarrierStatus>;
  { R.restoreBarrier(std::declval<typename T::BarrierStatus>()) };
};
// clang-format on

template <reader ReaderImpl> class WASMReader {
  using CursorStatus = typename ReaderImpl::CursorStatus;
  using BarrierStatus = typename ReaderImpl::BarrierStatus;

  ReaderImpl &R;

  template <std::integral T> static constexpr auto ceil(double Number) -> T {
    return (static_cast<double>(static_cast<T>(Number)) == Number)
               ? static_cast<T>(Number)
               : static_cast<T>(Number) + ((Number > 0) ? 1 : 0);
  }
  template <
      std::integral T,
      std::size_t MaxWidth_ = ceil<std::size_t>(sizeof(T) * 8 / 7.0)>
  T readLEB128() requires std::is_signed_v<T>;

  template <
      std::integral T,
      std::size_t MaxWidth_ = ceil<std::size_t>(sizeof(T) * 8 / 7.0)>
  T readLEB128() requires std::is_unsigned_v<T>;

public:
  explicit WASMReader(ReaderImpl &R_) : R(R_) {}

  std::byte read() { return R.read(); }
  std::span<std::byte const> read(std::size_t Size) { return R.read(Size); }
  void skip(std::size_t Size) { R.skip(Size); }
  std::byte peek() { return R.peek(); }
  std::size_t getNumBytesConsumed() { return R.getNumBytesConsumed(); }
  bool hasMoreBytes() { return R.hasMoreBytes(); }

  void resetBarrier() { R.resetBarrier(); }
  void setBarrier(std::size_t NumBytesAhead) { R.setBarrier(NumBytesAhead); }

  BarrierStatus backupBarrier() { return R.backupBarrier(); }
  template <typename T> void restoreBarrier(T &&Status) {
    return R.restoreBarrier(std::forward<T>(Status));
  }
  CursorStatus backupCursor() const { return R.backupCursor(); }
  template <typename T> void restoreCursor(T &&Status) {
    R.restoreCursor(std::forward<T>(Status));
  }

  // clang-format off
  std::int8_t   readSLEB128Int8 () { return readLEB128<std::int8_t  >(); }
  std::int16_t  readSLEB128Int16() { return readLEB128<std::int16_t >(); }
  std::int32_t  readSLEB128Int32() { return readLEB128<std::int32_t >(); }
  std::int64_t  readSLEB128Int64() { return readLEB128<std::int64_t >(); }
  std::uint8_t  readULEB128Int8 () { return readLEB128<std::uint8_t >(); }
  std::uint16_t readULEB128Int16() { return readLEB128<std::uint16_t>(); }
  std::uint32_t readULEB128Int32() { return readLEB128<std::uint32_t>(); }
  std::uint64_t readULEB128Int64() { return readLEB128<std::uint64_t>(); }

  std::string_view           readUTF8StringVector();

  bytecode::ValueType        readValueType();
  bytecode::FunctionType     readFunctionType();
  bytecode::MemoryType       readMemoryType();
  bytecode::TableType        readTableType();
  bytecode::GlobalType       readGlobalType();

  bytecode::ImportDescriptor readImportDescriptor();
  bytecode::ExportDescriptor readExportDescriptor();

  bytecode::BlockResultType  readBlockResultType();

  bytecode::TypeIDX readTypeIDX()
  { return static_cast<bytecode::TypeIDX  >(readULEB128Int32()); }
  bytecode::FuncIDX readFuncIDX()
  { return static_cast<bytecode::FuncIDX  >(readULEB128Int32()); }
  bytecode::TableIDX readTableIDX()
  { return static_cast<bytecode::TableIDX >(readULEB128Int32()); }
  bytecode::MemIDX readMemIDX()
  { return static_cast<bytecode::MemIDX   >(readULEB128Int32()); }
  bytecode::GlobalIDX readGlobalIDX()
  { return static_cast<bytecode::GlobalIDX>(readULEB128Int32()); }
  bytecode::LocalIDX readLocalIDX()
  { return static_cast<bytecode::LocalIDX >(readULEB128Int32()); }
  bytecode::LabelIDX readLabelIDX()
  { return static_cast<bytecode::LabelIDX >(readULEB128Int32()); }
  // clang-format on
};

template <reader ReaderImpl>
template <std::integral T, std::size_t MaxWidth_>
T WASMReader<ReaderImpl>::readLEB128() requires std::is_signed_v<T> {
  using UnsignedT = std::make_unsigned_t<T>;
  auto EnteringCursorStatus = backupCursor();
  auto MaximumWidth = MaxWidth_;
  UnsignedT Value = 0;
  unsigned ShiftAmount = 0;
  std::byte Byte;
  do {
    if (MaximumWidth == 0) {
      restoreCursor(EnteringCursorStatus);
      throw ParserError("SLEB128 decoding exceeds maximum number of bytes");
    }
    Byte = read();
    auto ByteSlice = static_cast<UnsignedT>(Byte) & 0x7f;
    Value = Value | (ByteSlice << ShiftAmount);
    ShiftAmount = ShiftAmount + 7;
  } while (static_cast<unsigned>(Byte) >= 0b10000000);
  if ((static_cast<unsigned>(Byte) & 0b01000000u) &&
      (ShiftAmount < sizeof(T) * 8))
    Value = Value | (static_cast<UnsignedT>(static_cast<T>(-1)) << ShiftAmount);
  return static_cast<T>(Value);
}

template <reader ReaderImpl>
template <std::integral T, std::size_t MaxWidth_>
T WASMReader<ReaderImpl>::readLEB128() requires std::is_unsigned_v<T> {
  auto EnteringCursorStatus = backupCursor();
  auto MaximumWidth = MaxWidth_;
  T Value = 0;
  unsigned ShiftAmount = 0;
  std::byte Byte;
  do {
    if (MaximumWidth == 0) {
      restoreCursor(EnteringCursorStatus);
      throw ParserError("ULEB128 decoding exceeds maximum number of bytes");
    }
    Byte = read();
    MaximumWidth = MaximumWidth - 1;
    T ByteSlice = static_cast<T>(Byte) & 0x7f;
    Value = Value | (ByteSlice << ShiftAmount);
    ShiftAmount = ShiftAmount + 7;
  } while (static_cast<unsigned>(Byte) >= 0b10000000);
  return Value;
}

template <reader ReaderImpl>
std::string_view WASMReader<ReaderImpl>::readUTF8StringVector() {
  auto EnteringCursorStatus = backupCursor();
  auto StringSize = readULEB128Int32();
  auto Bytes = read(StringSize);
  /* std::byte const * -> char const * */
  auto const *DataPtr = reinterpret_cast<char const *>(Bytes.data()); // NOLINT
  std::string_view StringView(DataPtr, StringSize);
  auto InvalidFindIter =
      utf8::find_invalid(StringView.begin(), StringView.end());
  if (InvalidFindIter != StringView.end()) {
    restoreCursor(EnteringCursorStatus);
    throw ParserError(fmt::format(
        "invalid utf8 encoding at offset {}",
        std::distance(StringView.begin(), InvalidFindIter)));
  }
  return StringView;
}

template <reader ReaderImpl>
bytecode::ValueType WASMReader<ReaderImpl>::readValueType() {
  auto ValueTypeByte = read();
  switch (static_cast<unsigned>(ValueTypeByte)) {
  case 0x7f: return bytecode::valuetypes::I32;
  case 0x7e: return bytecode::valuetypes::I64;
  case 0x7d: return bytecode::valuetypes::F32;
  case 0x7c: return bytecode::valuetypes::F64;
  default:
    throw ParserError(
        fmt::format("unknown value type byte 0x{:02x}", ValueTypeByte));
  }
}

template <reader ReaderImpl>
bytecode::FunctionType WASMReader<ReaderImpl>::readFunctionType() {
  using namespace utility::literals;
  auto MagicNumber = read();
  if (MagicNumber != 0x60_byte)
    throw ParserError(fmt::format(
        "mismatched function type magic number, expecting 0x60, but 0x{:02x} "
        "found",
        MagicNumber));
  std::vector<bytecode::ValueType> ParamTypes;
  std::vector<bytecode::ValueType> ResultTypes;
  auto NumParamTypes = readULEB128Int32();
  ParamTypes.reserve(NumParamTypes);
  for (decltype(NumParamTypes) I = 0; I < NumParamTypes; ++I)
    ParamTypes.push_back(readValueType());
  auto NumResultTypes = readULEB128Int32();
  for (decltype(NumResultTypes) I = 0; I < NumResultTypes; ++I)
    ResultTypes.push_back(readValueType());
  return bytecode::FunctionType(std::move(ParamTypes), std::move(ResultTypes));
}

template <reader ReaderImpl>
bytecode::MemoryType WASMReader<ReaderImpl>::readMemoryType() {
  auto MagicNumber = read();
  switch (static_cast<unsigned>(MagicNumber)) {
  case 0x00: {
    auto Min = readULEB128Int32();
    return bytecode::MemoryType(Min);
  }
  case 0x01: {
    auto Min = readULEB128Int32();
    auto Max = readULEB128Int32();
    return bytecode::MemoryType(Min, Max);
  }
  default:
    throw ParserError(fmt::format(
        "mismatched memory type magic number, expecting 0x00 or 0x01, but "
        "0x{:02x} found",
        MagicNumber));
  }
}

template <reader ReaderImpl>
bytecode::TableType WASMReader<ReaderImpl>::readTableType() {
  using namespace utility::literals;
  auto ElemTypeByte = read();
  if (ElemTypeByte != 0x70_byte)
    throw ParserError(fmt::format(
        "table type need to have type funcref(0x70), but 0x{:02x} found",
        ElemTypeByte));
  auto MagicNumber = read();
  switch (static_cast<unsigned>(MagicNumber)) {
  case 0x00: {
    auto Min = readULEB128Int32();
    return bytecode::TableType(Min);
  }
  case 0x01: {
    auto Min = readULEB128Int32();
    auto Max = readULEB128Int32();
    return bytecode::TableType(Min, Max);
  }
  default:
    throw ParserError(fmt::format(
        "mismatched table type magic number, expecting 0x00 or 0x01, but "
        "0x{:02x} found",
        MagicNumber));
  }
}

template <reader ReaderImpl>
bytecode::GlobalType WASMReader<ReaderImpl>::readGlobalType() {
  auto Type = readValueType();
  auto MutabilityByte = read();
  switch (static_cast<unsigned>(MutabilityByte)) {
  case 0x00: return bytecode::GlobalType(bytecode::MutabilityKind::Const, Type);
  case 0x01: return bytecode::GlobalType(bytecode::MutabilityKind::Var, Type);
  default:
    throw ParserError(fmt::format(
        "unknown mutability descriptor 0x{:02x} found in global type",
        MutabilityByte));
  }
}

template <reader ReaderImpl>
bytecode::ImportDescriptor WASMReader<ReaderImpl>::readImportDescriptor() {
  auto MagicNumber = read();
  switch (static_cast<unsigned>(MagicNumber)) {
  case 0x00: return readTypeIDX();
  case 0x01: return readTableType();
  case 0x02: return readMemoryType();
  case 0x03: return readGlobalType();
  default:
    throw ParserError(fmt::format(
        "unknown import descriptor magic number 0x{:02x}", MagicNumber));
  }
}

template <reader ReaderImpl>
bytecode::ExportDescriptor WASMReader<ReaderImpl>::readExportDescriptor() {
  auto MagicNumber = read();
  switch (static_cast<unsigned>(MagicNumber)) {
  case 0x00: return readFuncIDX();
  case 0x01: return readTableIDX();
  case 0x02: return readMemIDX();
  case 0x03: return readGlobalIDX();
  default:
    throw ParserError(fmt::format(
        "unknown export descriptor magic number 0x{:02x}", MagicNumber));
  }
}

template <reader ReaderImpl>
bytecode::BlockResultType WASMReader<ReaderImpl>::readBlockResultType() {
  using namespace utility::literals;
  auto FirstByte = peek();
  if ((FirstByte & 0b11000000_byte) == 0b01000000_byte) {
    // if this is a LEB encoding integer, it will be negative. Thus, it must be
    // either a unit block or it returns some value type.
    if (FirstByte == 0x40_byte) {
      utility::ignore(read()); // consume the byte
      return bytecode::BlockResultUnit{};
    }
    return readValueType();
  }
  auto Index = readLEB128<std::int64_t, 5>(); // read integer of type s33
  if (!((Index > 0) && (Index < std::numeric_limits<std::uint32_t>::max())))
    throw ParserError(
        "function index in block type beyonds maximum possible value");
  return static_cast<bytecode::TypeIDX>(Index);
}
} // namespace parser

#endif
