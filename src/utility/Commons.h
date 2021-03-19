#ifndef SABLE_INCLUDE_GUARD_UTILITY_COMMONS
#define SABLE_INCLUDE_GUARD_UTILITY_COMMONS

#include <cstddef>

#if !defined(__has_builtin)
#define __has_builtin(x) 0
#endif

#if !__has_builtin(__builtin_trap)
#include <cstdlib>
#endif

#if defined(__GNUC__)
#define ATTRIBUTE(x) __attribute__(x)
#else
#define ATTRIBUTE(x)
#endif

namespace utility::literals {
constexpr std::byte operator""_byte(unsigned long long X) {
  return static_cast<std::byte>(X);
}
} // namespace utility::literals

namespace utility {
[[noreturn]] inline void unreachable() {
#if __has_builtin(__builtin_trap)
  __builtin_trap();
#else
  std::abort();
#endif
}
template <typename... ArgTypes> void ignore(ArgTypes &&.../* IGNORED */) {}

template <typename... Ts> struct Overload : Ts... { using Ts::operator()...; };
template <typename... Ts> Overload(Ts...) -> Overload<Ts...>;
} // namespace utility
#endif
