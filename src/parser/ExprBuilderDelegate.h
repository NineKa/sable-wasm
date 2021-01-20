#ifndef SABLE_INCLUDE_GUARD_PARSER_EXPR_BUILDER_DELEGATE
#define SABLE_INCLUDE_GUARD_PARSER_EXPR_BUILDER_DELEGATE

#include "../bytecode/Instruction.h"
#include "../bytecode/Type.h"
#include "Delegate.h"

#include <stack>

namespace parser {
class ExprBuilderDelegate : public DelegateBase {
  bytecode::Expression CExpr;
  std::stack<bytecode::Expression> Scopes;
  template <bytecode::instruction T> T *getCurrentScopeEnclosingInst();
  template <bytecode::instruction T, typename... ArgTypes>
  void addInst(ArgTypes &&...Args);

public:
#define SABLE_SKIP_SECTION_EVENTS
#define MAKE_DELEGATE_ARG(r, data, elem) BOOST_PP_TUPLE_ELEM(2, 0, elem)
#define GENERATE_DELEGATE_ARGS(Name, MemberArray)                              \
  void Name(BOOST_PP_LIST_ENUM(BOOST_PP_LIST_TRANSFORM(                        \
      MAKE_DELEGATE_ARG, BOOST_PP_EMPTY,                                       \
      BOOST_PP_ARRAY_TO_LIST(MemberArray))));
#define X(Name, MemberArray) GENERATE_DELEGATE_ARGS(Name, MemberArray)
#include "DelegateEvents.defs"
#undef X
#undef GENERATE_DELEGATE_ARGS
#undef MAKE_DELEGATE_ARG
#undef SABLE_SKIP_SECTION_EVENTS

  bytecode::Expression &getExpression() { return CExpr; }
};
} // namespace parser

#endif
