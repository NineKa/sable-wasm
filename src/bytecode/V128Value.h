#ifndef SABLE_INCLUDE_GUARD_BYTECODE_V128_VALUE
#define SABLE_INCLUDE_GUARD_BYTECODE_V128_VALUE

#include <range/v3/algorithm/copy_n.hpp>
#include <range/v3/algorithm/fill_n.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <span>

namespace bytecode {

class V128Value {
  alignas(16) std::array<std::byte, 16> Storage;

  template <typename T> std::span<T, 16 / sizeof(T)> as() {
    auto *BufferPtr = reinterpret_cast<T *>(Storage.data()); // NOLINT
    return std::span<T, 16 / sizeof(T)>(BufferPtr, 16 / sizeof(T));
  }

  template <typename T> std::span<T const, 16 / sizeof(T)> as() const {
    auto const *BufferPtr =
        reinterpret_cast<T const *>(Storage.data()); // NOLINT
    return std::span<T const, 16 / sizeof(T)>(BufferPtr, 16 / sizeof(T));
  }

public:
  V128Value() : Storage() { ranges::fill_n(Storage.begin(), 16, std::byte(0)); }

  explicit V128Value(std::span<std::byte const, 16> Value) : Storage() {
    ranges::copy_n(Value.begin(), 16, Storage.begin());
  }

  explicit V128Value(std::span<std::byte const> Value) : Storage() {
    assert(Value.size() == 16);
    ranges::copy_n(Value.begin(), 16, Storage.begin());
  }

  explicit V128Value(std::span<std::int8_t const, 16> Value)
      : V128Value(std::as_bytes(Value)) {}
  explicit V128Value(std::span<std::uint8_t const, 16> Value)
      : V128Value(std::as_bytes(Value)) {}
  explicit V128Value(std::span<std::int16_t const, 8> Value)
      : V128Value(std::as_bytes(Value)) {}
  explicit V128Value(std::span<std::uint16_t const, 8> Value)
      : V128Value(std::as_bytes(Value)) {}
  explicit V128Value(std::span<std::int32_t const, 4> Value)
      : V128Value(std::as_bytes(Value)) {}
  explicit V128Value(std::span<std::uint32_t const, 4> Value)
      : V128Value(std::as_bytes(Value)) {}
  explicit V128Value(std::span<std::int64_t const, 2> Value)
      : V128Value(std::as_bytes(Value)) {}
  explicit V128Value(std::span<std::uint64_t const, 2> Value)
      : V128Value(std::as_bytes(Value)) {}
  explicit V128Value(std::span<float const, 4> Value)
      : V128Value(std::as_bytes(Value)) {}
  explicit V128Value(std::span<double const, 2> Value)
      : V128Value(std::as_bytes(Value)) {}

  // clang-format off
  std::span<std::int8_t  , 16> asI8x16() { return as<std::int8_t  >(); }
  std::span<std::uint8_t , 16> asU8x16() { return as<std::uint8_t >(); }
  std::span<std::int16_t ,  8> asI16x8() { return as<std::int16_t >(); }
  std::span<std::uint16_t,  8> asU16x8() { return as<std::uint16_t>(); }
  std::span<std::int32_t ,  4> asI32x4() { return as<std::int32_t >(); }
  std::span<std::uint32_t,  4> asU32x4() { return as<std::uint32_t>(); }
  std::span<std::int64_t ,  2> asI64x2() { return as<std::int64_t >(); }
  std::span<std::uint64_t,  2> asU64x2() { return as<std::uint64_t>(); }
  std::span<float , 4> asF32x4() { return as<float >(); }
  std::span<double, 2> asF64x2() { return as<double>(); }

  std::span<std::int8_t   const, 16> asI8x16() const
  { return as<std::int8_t  >(); }
  std::span<std::uint8_t  const, 16> asU8x16() const
  { return as<std::uint8_t >(); }
  std::span<std::int16_t  const,  8> asI16x8() const
  { return as<std::int16_t >(); }
  std::span<std::uint16_t const,  8> asU16x8() const
  { return as<std::uint16_t>(); }
  std::span<std::int32_t  const,  4> asI32x4() const
  { return as<std::int32_t >(); }
  std::span<std::uint32_t const,  4> asU32x4() const
  { return as<std::uint32_t>(); }
  std::span<std::int64_t  const,  2> asI64x2() const
  { return as<std::int64_t >(); }
  std::span<std::uint64_t const,  2> asU64x2() const
  { return as<std::uint64_t>(); }
  std::span<float  const, 4> asF32x4() const { return as<float >(); }
  std::span<double const, 2> asF64x2() const { return as<double>(); }
  // clang-format on
};
} // namespace bytecode

#endif