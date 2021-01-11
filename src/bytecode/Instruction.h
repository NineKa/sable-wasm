#ifndef SABLE_INCLUDE_GUARD_BYTECODE_INSTRUCTION
#define SABLE_INCLUDE_GUARD_BYTECODE_INSTRUCTION

#include "../utility/Commons.h"
#include "Type.h"

#include <boost/preprocessor.hpp>
#include <fmt/format.h>
#include <poly/vector.h>

#include <cstdint>
#include <variant>

namespace bytecode {
enum class Opcode {
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

class Instruction {
  Opcode InstOpcode;

protected:
  explicit Instruction(Opcode InstOpcode_) : InstOpcode(InstOpcode_) {}

public:
  Instruction(Instruction const &) = default;
  Instruction(Instruction &&) noexcept = default;
  Instruction &operator=(Instruction const &) = default;
  Instruction &operator=(Instruction &&) noexcept = default;
  virtual ~Instruction() noexcept = default;
  Opcode getOpcode() const { return InstOpcode; }
};

using Expression = poly::vector<Instruction>;

// clang-format off
enum class LabelIDX  : std::uint32_t {};
enum class LocalIDX  : std::uint32_t {};
enum class GlobalIDX : std::uint32_t {};
enum class TableIDX  : std::uint32_t {};
enum class MemIDX    : std::uint32_t {};
enum class FuncIDX   : std::uint32_t {};
enum class TypeIDX   : std::uint32_t {};
// clang-format on

struct BlockTypeUnit {};
using BlockType = std::variant<ValueType, TypeIDX, BlockTypeUnit>;

namespace instructions {
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
  struct Name : public Instruction {                                           \
    explicit Name(GENERATE_CTOR_PARAMS(Name, MemberArray))                     \
        : GENERATE_CTOR_INITIALIZERS(Name, MemberArray) {}                     \
    BOOST_PP_LIST_FOR_EACH(                                                    \
        MAKE_MEMBER_DECL, _, BOOST_PP_ARRAY_TO_LIST(MemberArray))              \
    static constexpr InstCategory getCategory() {                              \
      return InstCategory::Category;                                           \
    }                                                                          \
    static constexpr char const *getNameString() { return NameString; }        \
    static constexpr Opcode getOpcode() { return Opcode::Name; }               \
    static bool classof(Instruction const &Other) {                            \
      return getOpcode() == Other.getOpcode();                                 \
    }                                                                          \
  };
#include "Instruction.defs"
#undef X
#undef GENERATE_CTOR_INITIALIZERS
#undef MAKE_CTOR_INITIALIZER
#undef GENERATE_CTOR_PARAMS
#undef MAKE_CTOR_PARAM
#undef MAKE_MEMBER_DECL
} // namespace instructions

template <typename Derived, typename RetType = void, bool Const = true>
class InstVisitorBase {
  Derived &derived() { return static_cast<Derived &>(*this); }
  Derived const &derived() const { return static_cast<Derived const &>(*this); }
  using InstRef = std::conditional_t<Const, Instruction const &, Instruction &>;

public:
  RetType visit(InstRef Inst) {
    switch (Inst.getOpcode()) {
#define X(Name, NameString, Category, OpcodeSeq, MemberArray)                  \
  case Opcode::Name:                                                           \
    return derived()(static_cast<instructions::Name const &>(Inst));
#include "Instruction.defs"
#undef X
    default: SABLE_UNREACHABLE();
    }
  }
};

namespace detail {
template <typename T, InstCategory Category>
constexpr bool with_category_tag = T::getCategory() == Category;
} // namespace detail

// clang-format off
template <typename T>
concept instruction = std::derived_from<T, Instruction>
  && std::is_same_v<T, std::remove_cv_t<T>> // no cv qualifiers
  && requires (T const & Other) {
    { T::getCategory()   } -> std::convertible_to<InstCategory>;
    { T::getNameString() } -> std::convertible_to<char const *>;
    { T::getOpcode()     } -> std::convertible_to<Opcode>;
    { T::classof(Other)  } -> std::convertible_to<bool>;
};

template <typename T, InstCategory Category>
concept instruction_in_category  = instruction<T>
  && detail::with_category_tag<T, Category>;
// clang-format on

template <instruction T> bool is_a(Instruction const &Inst) {
  return T::classof(Inst);
}
} // namespace bytecode

////////////////////////////////// Formatters //////////////////////////////////
template <> struct fmt::formatter<bytecode::Instruction, char> {
  template <typename Iterator>
  struct InstFormatVisitor
      : bytecode::InstVisitorBase<InstFormatVisitor<Iterator>, Iterator> {
    Iterator Out;
    explicit InstFormatVisitor(Iterator Out_) : Out(Out_) {}
    template <bytecode::instruction T>
    Iterator operator()(T const & /* IGNORE */) {
      return fmt::format_to(Out, "{}", T::getNameString());
    }
  };
  template <typename C> constexpr auto parse(C &&CTX) { return CTX.begin(); }
  template <typename Context>
  auto format(bytecode::Instruction const &Inst, Context &&CTX) {
    InstFormatVisitor Visitor(CTX.out());
    return Visitor.visit(Inst);
  }
};

template <typename T>
struct fmt::formatter<T, char, std::enable_if_t<bytecode::instruction<T>>> {
  template <typename C> constexpr auto parse(C &&CTX) { return CTX.begin(); }
  template <typename Context>
  auto format(T const & /* IGNORE */, Context &&CTX) {
    return fmt::format_to(CTX.out(), "{}", T::getNameString());
  }
};
#endif