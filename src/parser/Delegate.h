#ifndef SABLE_INCLUDE_GUARD_PARSER_DELEGATE
#define SABLE_INCLUDE_GUARD_PARSER_DELEGATE

#include "Reader.h"

#include <boost/preprocessor.hpp>
#include <cstdint>
#include <utility>

namespace parser {
using SizeType = std::uint32_t;

#define MAKE_CONSTRAIN_ARG(r, data, elem)                                      \
  std::declval<BOOST_PP_TUPLE_ELEM(2, 0, elem)>()
#define GENERATE_CONSTRAIN_ARGS(Name, MemberArray)                             \
  {D.Name(BOOST_PP_LIST_ENUM(BOOST_PP_LIST_TRANSFORM(                          \
      MAKE_CONSTRAIN_ARG, BOOST_PP_EMPTY,                                      \
      BOOST_PP_ARRAY_TO_LIST(MemberArray))))};
// clang-format off
template <typename T>
concept delegate = requires(T D) {
#define X(Name, MemberArray) GENERATE_CONSTRAIN_ARGS(Name, MemberArray)
#include "DelegateEvents.defs"
#undef X
};
// clang-format on
#undef GENERATE_CONSTRAIN_ARGS
#undef MAKE_CONSTRAIN_ARG

#define MAKE_DELEGATE_ARG(r, data, elem) BOOST_PP_TUPLE_ELEM(2, 0, elem) const &
#define GENERATE_DELEGATE_ARGS(Name, MemberArray)                              \
  void Name(BOOST_PP_LIST_ENUM(BOOST_PP_LIST_TRANSFORM(                        \
      MAKE_DELEGATE_ARG, BOOST_PP_EMPTY,                                       \
      BOOST_PP_ARRAY_TO_LIST(MemberArray)))) {}
struct DelegateBase {
#define X(Name, MemberArray) GENERATE_DELEGATE_ARGS(Name, MemberArray)
#include "DelegateEvents.defs"
#undef X
};
#undef GENERATE_DELEGATE_ARGS
#undef MAKE_DELEGATE_ARG

static_assert(delegate<DelegateBase>);

} // namespace parser

#endif
