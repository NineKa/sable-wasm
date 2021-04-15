#ifndef SABLE_INCLUDE_GUARD_UTILITY_COMMONS
#define SABLE_INCLUDE_GUARD_UTILITY_COMMONS

#include <chrono>
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

inline void expect(bool Condition) {
  if (!Condition) unreachable();
}

template <typename... Ts> struct Overload : Ts... { using Ts::operator()...; };
template <typename... Ts> Overload(Ts...) -> Overload<Ts...>;

template <typename T> auto measure(T &&Fn) {
  auto Start = std::chrono::high_resolution_clock::now();
  Fn();
  auto Stop = std::chrono::high_resolution_clock::now();
  return Stop - Start;
}
} // namespace utility

#define SABLE_DEFINE_IS_A(BaseType)                                            \
  template <typename T> bool is_a(BaseType const *Ptr) {                       \
    return T::classof(Ptr);                                                    \
  }                                                                            \
  template <typename T> bool is_a(BaseType const &Ref) {                       \
    return T::classof(std::addressof(Ref));                                    \
  }

#define SABLE_DEFINE_DYN_CAST(BaseType)                                        \
  template <typename T> T *dyn_cast(BaseType *Ptr) {                           \
    if (Ptr == nullptr) return nullptr;                                        \
    assert(is_a<T>(Ptr));                                                      \
    return static_cast<T *>(Ptr);                                              \
  }                                                                            \
                                                                               \
  template <typename T> T &dyn_cast(BaseType &Ref) {                           \
    assert(is_a<T>(Ref));                                                      \
    return static_cast<T &>(Ref);                                              \
  }                                                                            \
                                                                               \
  template <typename T> T const *dyn_cast(BaseType const *Ptr) {               \
    if (Ptr == nullptr) return nullptr;                                        \
    assert(is_a<T>(Ptr));                                                      \
    return static_cast<T const *>(Ptr);                                        \
  }                                                                            \
                                                                               \
  template <typename T> T const &dyn_cast(BaseType const &Ref) {               \
    assert(is_a<T>(Ref));                                                      \
    return static_cast<T const &>(Ref);                                        \
  }
#endif
