#ifndef SABLE_INCLUDE_GUARD_UTILITY_COMMONS
#define SABLE_INCLUDE_GUARD_UTILITY_COMMONS

#include <iterator>
#include <ranges>

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

template <std::forward_iterator Iterator>
class IteratorPair : std::ranges::view_base {
  using Trait = std::iterator_traits<Iterator>;
  Iterator Begin;
  Iterator End;

public:
  using type = Iterator;
  using difference_type = typename Trait::difference_type;
  using value_type = typename Trait::value_type;
  using pointer = typename Trait::pointer;
  using reference = typename Trait::reference;
  using iterator_category = typename Trait::iterator_category;

  constexpr IteratorPair(Iterator Begin_, Iterator End_)
      : Begin(Begin_), End(End_) {}
  constexpr Iterator begin() const { return Begin; }
  constexpr Iterator end() const { return End; }
  difference_type size() const { return std::distance(begin(), end()); }
};
} // namespace utility

#endif