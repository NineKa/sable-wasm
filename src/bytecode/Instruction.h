#ifndef SABLE_INCLUDE_GUARD_BYTECODE_INSTRUCTION
#define SABLE_INCLUDE_GUARD_BYTECODE_INSTRUCTION

#include "../utility/Commons.h"
#include "Type.h"

#include <boost/preprocessor.hpp>
#include <fmt/format.h>

#include <cstdint>
#include <unordered_map>
#include <variant>

namespace bytecode {
enum class Opcode : std::uint16_t {
#define X(Name, NameString, Category, OpcodeSeq, MemberArray) Name,
#include "Instruction.defs"
#undef X
};

enum class InstCategory {
  Control,
  Parametric,
  Variable,
  Memory,
  Numeric,
  /* sign-extension operators (merged WG-03-11) */
  SignExtensionOps,
  /* nontrapping float-to-int conversions (merged WG-03-11) */
  NontrappingFloatToIntConvs
};

class TaggedInstPtr;
class Instruction {
  Opcode InstOpcode;
  friend class TaggedInstPtr;
  virtual std::size_t getSize() const = 0;

protected:
  explicit Instruction(Opcode InstOpcode_) : InstOpcode(InstOpcode_) {}
  Instruction(Instruction const &) = default;
  Instruction(Instruction &&) noexcept = default;
  Instruction &operator=(Instruction const &) = default;
  Instruction &operator=(Instruction &&) noexcept = default;

public:
  virtual ~Instruction() noexcept = default;
  Opcode getOpcode() const { return InstOpcode; }
};

// clang-format off
enum class LabelIDX  : std::uint32_t {};
enum class LocalIDX  : std::uint32_t {};
enum class GlobalIDX : std::uint32_t {};
enum class TableIDX  : std::uint32_t {};
enum class MemIDX    : std::uint32_t {};
enum class FuncIDX   : std::uint32_t {};
enum class TypeIDX   : std::uint32_t {};
// clang-format on

struct BlockResultTypeUnit {};
using BlockResultType = std::variant<ValueType, TypeIDX, BlockResultTypeUnit>;

template <typename T> struct inst_trait;

namespace detail {
template <typename T, InstCategory Category>
constexpr bool with_category_tag = inst_trait<T>::category() == Category;
} // namespace detail

// clang-format off
template <typename T>
concept instruction = std::derived_from<T, Instruction>
                      // no cv qualifiers
                      && std::is_same_v<T, std::remove_cv_t<T>> 
                      && requires (T const * Other) {
  { T::classof(Other)              } -> std::convertible_to<bool>;
  { inst_trait<T>::opcode()           } -> std::convertible_to<Opcode>;
  { inst_trait<T>::category()         } -> std::convertible_to<InstCategory>;
  { inst_trait<T>::getNameString()    } -> std::convertible_to<char const *>;
  { inst_trait<T>::hasNoImmediate()   } -> std::convertible_to<bool>;
};

template <typename T, InstCategory Category>
concept instruction_in_category  = instruction<T>
                                   && detail::with_category_tag<T, Category>;
// clang-format on

template <instruction T> bool is_a(Instruction const *Inst) {
  return T::classof(Inst);
}

namespace detail {
template <typename T>
concept inst_with_no_immediate = instruction<T> &&
inst_trait<T>::hasNoImmediate();
template <inst_with_no_immediate T> static T NoImmInstSingleton;
} // namespace detail

class TaggedInstPtr {
  std::intptr_t InstPtr;
  explicit TaggedInstPtr(std::intptr_t InstPtr_) : InstPtr(InstPtr_) {}
  static std::intptr_t getIntPtrNull() {
    return reinterpret_cast<std::intptr_t>(nullptr);
  }

public:
  TaggedInstPtr(TaggedInstPtr const &) = delete;
  TaggedInstPtr(TaggedInstPtr &&Other) noexcept : InstPtr(Other.InstPtr) {
    Other.InstPtr = getIntPtrNull();
  }
  TaggedInstPtr &operator=(TaggedInstPtr const &) = delete;
  TaggedInstPtr &operator=(TaggedInstPtr &&Other) noexcept {
    InstPtr = Other.InstPtr;
    Other.InstPtr = getIntPtrNull();
    return *this;
  }
  ~TaggedInstPtr() noexcept {
    if (InstPtr == getIntPtrNull()) return;
    if (isHeapAllocated()) { delete asPointer(); }
  }

  operator Instruction *() const { return asPointer(); }
  Instruction &operator*() { return *asPointer(); }
  Instruction *operator->() { return asPointer(); }

  Instruction *asPointer() const {
    auto UnmaskedPtr = InstPtr & ~(std::intptr_t(0x01));
    return reinterpret_cast<Instruction *>(UnmaskedPtr);
  }

  bool isNull() const { return InstPtr == getIntPtrNull(); }
  bool isHeapAllocated() const { return (InstPtr & 0x01) == 0; }

  template <instruction T, typename... ArgTypes>
  static TaggedInstPtr Build(ArgTypes &&...Args) {
    static_assert(alignof(T) >= 2, "use lowest bit as 'tagged bit'");
    if constexpr (inst_trait<T>::hasNoImmediate()) {
      T *Address = std::addressof(detail::NoImmInstSingleton<T>);
      auto IntPtr = reinterpret_cast<std::intptr_t>(Address);
      assert((IntPtr & 0x01) == 0);
      IntPtr = IntPtr | 0x01;
      return TaggedInstPtr(IntPtr);
    } else {
      auto *Address = new T(std::forward<ArgTypes>(Args)...);
      auto IntPtr = reinterpret_cast<std::intptr_t>(Address);
      assert((IntPtr & 0x01) == 0);
      return TaggedInstPtr(IntPtr);
    }
  }
};

class Expression {
  std::vector<TaggedInstPtr> Storage;

public:
  Expression() {}
  Expression(Expression const &) = delete;
  Expression(Expression &&) noexcept = default;
  Expression &operator=(Expression const &) = delete;
  Expression &operator=(Expression &&) noexcept = default;

  using value_type = TaggedInstPtr;
  using reference = value_type &;
  using const_reference = value_type const &;
  using iterator = decltype(Storage)::iterator;
  using const_iterator = decltype(Storage)::const_iterator;
  using reverse_iterator = decltype(Storage)::reverse_iterator;
  using const_reverse_iterator = decltype(Storage)::const_reverse_iterator;

  std::size_t size() const { return Storage.size(); }
  bool empty() const { return Storage.empty(); }
  void clear() { Storage.clear(); }

  reference back() { return Storage.back(); }
  const_reference back() const { return Storage.back(); }

  template <instruction T, typename... ArgTypes>
  reference emplace_back(ArgTypes &&...Args) {
    auto TaggedPtr = TaggedInstPtr::Build<T>(std::forward<ArgTypes>(Args)...);
    Storage.push_back(std::move(TaggedPtr));
    return Storage.back();
  }

  iterator begin() { return Storage.begin(); }
  iterator end() { return Storage.end(); }
  const_iterator begin() const { return Storage.begin(); }
  const_iterator end() const { return Storage.end(); }
  reverse_iterator rbegin() { return Storage.rbegin(); }
  reverse_iterator rend() { return Storage.rend(); }
  const_reverse_iterator rbegin() const { return Storage.rbegin(); }
  const_reverse_iterator rend() const { return Storage.rend(); }
};

#define MAKE_MEMBER_DECL(r, data, elem)                                        \
  BOOST_PP_TUPLE_ELEM(2, 0, elem) BOOST_PP_TUPLE_ELEM(2, 1, elem);
#define MAKE_CTOR_PARAM(d, data, elem)                                         \
  BOOST_PP_TUPLE_ELEM(2, 0, elem)                                              \
  BOOST_PP_CAT(BOOST_PP_TUPLE_ELEM(2, 1, elem), _)
#define GENERATE_CTOR_PARAMS(Name, MemberArray)                                \
  BOOST_PP_LIST_ENUM(BOOST_PP_LIST_TRANSFORM(                                  \
      MAKE_CTOR_PARAM, BOOST_PP_EMPTY, BOOST_PP_ARRAY_TO_LIST(MemberArray)))
#define MAKE_CTOR_INITIALIZER(d, data, elem)                                   \
  BOOST_PP_TUPLE_ELEM(2, 1, elem)                                              \
  (std::move(BOOST_PP_CAT(BOOST_PP_TUPLE_ELEM(2, 1, elem), _)))
#define GENERATE_CTOR_INITIALIZERS(Name, MemberArray)                          \
  BOOST_PP_LIST_ENUM(BOOST_PP_LIST_APPEND(                                     \
      (Instruction(Opcode::Name), BOOST_PP_NIL),                               \
      BOOST_PP_LIST_TRANSFORM(                                                 \
          MAKE_CTOR_INITIALIZER, BOOST_PP_EMPTY,                               \
          BOOST_PP_ARRAY_TO_LIST(MemberArray))))
#define X(Name, NameString, Category, OpcodeSeq, MemberArray)                  \
  namespace instructions {                                                     \
  struct Name;                                                                 \
  }                                                                            \
  template <> struct inst_trait<instructions::Name> {                          \
    static constexpr Opcode opcode() { return Opcode::Name; }                  \
    static constexpr InstCategory category() {                                 \
      return InstCategory::Category;                                           \
    }                                                                          \
    static constexpr char const *getNameString() { return NameString; }        \
    static constexpr bool hasNoImmediate() {                                   \
      return BOOST_PP_IF(BOOST_PP_ARRAY_SIZE(MemberArray), false, true);       \
    }                                                                          \
  };                                                                           \
  namespace instructions {                                                     \
  struct Name : public Instruction {                                           \
    explicit Name(GENERATE_CTOR_PARAMS(Name, MemberArray))                     \
        : GENERATE_CTOR_INITIALIZERS(Name, MemberArray) {}                     \
    BOOST_PP_LIST_FOR_EACH(                                                    \
        MAKE_MEMBER_DECL, _, BOOST_PP_ARRAY_TO_LIST(MemberArray))              \
                                                                               \
    static bool classof(Instruction const *Other) {                            \
      assert(Other != nullptr);                                                \
      return inst_trait<Name>::opcode() == Other->getOpcode();                 \
    }                                                                          \
                                                                               \
  private:                                                                     \
    std::size_t getSize() const override { return sizeof(Name); }              \
  };                                                                           \
  } // namespace instructions
#include "Instruction.defs"
#undef X
#undef GENERATE_CTOR_INITIALIZERS
#undef MAKE_CTOR_INITIALIZER
#undef GENERATE_CTOR_PARAMS
#undef MAKE_CTOR_PARAM
#undef MAKE_MEMBER_DECL

template <typename Derived, typename RetType = void, bool Const = true>
class InstVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  Derived const &derived() const { return static_cast<Derived const &>(*this); }
  template <typename T> using Ptr = std::conditional_t<Const, T const *, T *>;

public:
  RetType visit(Ptr<Instruction> Inst) {
    switch (Inst->getOpcode()) {
#define X(Name, NameString, Category, OpcodeSeq, MemberArray)                  \
  case Opcode::Name:                                                           \
    return derived()(static_cast<Ptr<instructions::Name>>(Inst));
#include "Instruction.defs"
#undef X
    default: SABLE_UNREACHABLE();
    }
  }
};
} // namespace bytecode

////////////////////////////////// Formatters //////////////////////////////////
template <>
struct fmt::formatter<bytecode::Opcode> : fmt::formatter<char const *> {
  using BaseFormatter = fmt::formatter<char const *>;
  constexpr char const *getNameString(bytecode::Opcode Opcode) {
    switch (Opcode) {
#define X(Name, NameString, Category, OpcodeSeq, MemberArray)                  \
  case bytecode::Opcode::Name:                                                 \
    return NameString;
#include "Instruction.defs"
#undef X
    default: SABLE_UNREACHABLE();
    }
  }
  template <typename Context>
  auto format(bytecode::Opcode const &Opcode, Context &&CTX) {
    return BaseFormatter::format(
        getNameString(Opcode), std::forward<Context>(CTX));
  }
};

#endif