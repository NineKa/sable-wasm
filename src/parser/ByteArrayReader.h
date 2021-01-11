#ifndef SABLE_INCLUDE_GUARD_PARSER_BYTE_ARRAY_READER
#define SABLE_INCLUDE_GUARD_PARSER_BYTE_ARRAY_READER

#include "Reader.h"

#include <optional>
#include <span>

namespace parser {
class ByteArrayReader {
  std::span<std::byte const> Buffer;
  std::size_t Cursor;
  std::optional<std::size_t> Barrier;

  std::size_t assertValidPos(std::size_t CandidatePos) const {
    if (CandidatePos > Buffer.size())
      throw ParserError("reader attempt beyonds maximum length");
    if (Barrier.has_value() && (CandidatePos > *Barrier))
      throw ParserError("reader attempt beyonds barrier limit");
    return CandidatePos;
  }

public:
  explicit ByteArrayReader(std::span<std::byte const> Buffer_)
      : Buffer(Buffer_), Cursor(0), Barrier(std::nullopt) {}
  ByteArrayReader(std::byte const *Buffer_, std::size_t Size)
      : ByteArrayReader(std::span<std::byte const>(Buffer_, Size)) {}

  std::byte read() {
    auto UpdatedCursor = assertValidPos(Cursor + 1);
    auto Result = Buffer[Cursor];
    Cursor = UpdatedCursor;
    return Result;
  }

  std::span<std::byte const> read(std::size_t Size) {
    auto UpdatedCursor = assertValidPos(Cursor + Size);
    auto Result = Buffer.subspan(Cursor, Size);
    Cursor = UpdatedCursor;
    return Result;
  }

  std::byte peek() const {
    utility::ignore(assertValidPos(Cursor + 1));
    return Buffer[Cursor];
  }

  void skip(std::size_t NumBytes) {
    auto UpdatedCursor = assertValidPos(Cursor + NumBytes);
    Cursor = UpdatedCursor;
  }

  bool hasMoreBytes() const {
    if (Barrier.has_value() && (Cursor >= *Barrier)) return false;
    if (Cursor >= Buffer.size()) return false;
    return true;
  }

  std::size_t getNumBytesConsumed() const { return Cursor; }

  enum class CursorStatus : std::size_t {};

  CursorStatus backupCursor() const {
    return static_cast<CursorStatus>(Cursor);
  }

  void restoreCursor(CursorStatus const &Status) {
    Cursor = static_cast<std::size_t>(Status);
  }

  enum class BarrierStatus : std::size_t {};

  void setBarrier(std::size_t NumBytesAhead) {
    if ((Cursor + NumBytesAhead) > Buffer.size())
      throw ParserError("reader attempts to set barrier beyonds the end");
    Barrier = Cursor + NumBytesAhead;
  }

  void resetBarrier() { Barrier.reset(); }

  BarrierStatus backupBarrier() const {
    if (!Barrier.has_value())
      throw ParserError("reader barrier has not been set yet");
    return static_cast<BarrierStatus>(*Barrier);
  }

  void restoreBarrier(BarrierStatus const &Status) {
    Barrier = static_cast<std::size_t>(Status);
  }
};
} // namespace parser

#endif