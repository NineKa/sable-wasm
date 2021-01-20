#ifndef SABLE_INCLUDE_GUARD_UTILITY_COMMONS
#define SABLE_INCLUDE_GUARD_UTILITY_COMMONS

#include <iterator>
#include <ranges>
#include <variant>

#if !defined(__has_builtin)
#define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_trap)
#define SABLE_UNREACHABLE() __builtin_trap()
#else
#include <cstdlib>
#define SABLE_UNREACHABLE() std::abort()
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
template <typename... ArgTypes> void ignore(ArgTypes &&.../* IGNORED */) {}

template <typename... Ts> struct Overload : Ts... { using Ts::operator()...; };
template <typename... Ts> Overload(Ts...) -> Overload<Ts...>;

template <typename... ArgTypes> class Sum : std::variant<ArgTypes...> {
public:
  using std::variant<ArgTypes...>::variant;
  template <typename T> bool is() const {
    return std::holds_alternative<T>(*this);
  }
  template <typename T> T &as() { return std::get<T>(*this); }
  template <typename T> T const &as() const { return std::get<T>(*this); }
  template <typename Function> auto match(Function &&Fn) {
    return std::visit(std::forward<Function>(Fn), *this);
  }
  template <typename Function> auto match(Function &&Fn) const {
    return std::visit(std::forward<Function>(Fn), *this);
  }
};
} // namespace utility

#endif
