#ifndef SABLE_INCLUDE_GUARD_BYTECODE_VALIDATION
#define SABLE_INCLUDE_GUARD_BYTECODE_VALIDATION

#include "Instruction.h"
#include "Module.h"
#include "Type.h"

#include <variant>
#include <vector>

namespace bytecode::validation {
bool validate(ValueType const &Type);
bool validate(TableType const &Type);
bool validate(MemoryType const &Type);
bool validate(FunctionType const &Type);
bool validate(GlobalType const &Type);

class TypeVariable {
  std::size_t ID;

public:
  explicit TypeVariable(std::size_t ID_) : ID(ID_) {}
  std::size_t getID() const { return ID; }
  bool operator==(TypeVariable const &) const = default;
  auto operator<=>(TypeVariable const &) const = default;
};

using OperandStackElement = std::variant<ValueType, TypeVariable>;

enum class EntitySiteKind {
  Type,
  Function,
  Table,
  Memory,
  Global,
  Element,
  Data,
  Start,
  Import,
  Export
};

class ValidationError : std::runtime_error {
  EntitySiteKind ESiteKind;
  std::size_t ESiteIndex;
  std::vector<InstructionPtr const *> ISites;

public:
  template <ranges::input_range T>
  ValidationError(
      EntitySiteKind ESiteKind_, std::size_t ESiteIndex_, T const &ISites_)
      : std::runtime_error("validation error"), ESiteKind(ESiteKind_),
        ESiteIndex(ESiteIndex_),
        ISites(ranges::to<std::vector<InstructionPtr const *>>(ISites_)) {}
  EntitySiteKind getEntitySiteKind() const { return ESiteKind; }
  std::size_t getEntitySiteIndex() const { return ESiteIndex; }
  auto getInstSites() const { return ranges::views::all(ISites); }
  virtual void signal() = 0;
};

class TypeError : public ValidationError {
  bool Epsilon;
  std::vector<OperandStackElement> Expecting;
  std::vector<OperandStackElement> Actual;

public:
  template <ranges::input_range S, ranges::input_range T, ranges::input_range U>
  TypeError(
      EntitySiteKind ESiteKind_, std::size_t ESiteIndex_, S const &ISites_,
      bool Epsilon_, T const &Expecting_, U const &Actual_)
      : ValidationError(ESiteKind_, ESiteIndex_, ISites_), Epsilon(Epsilon_),
        Expecting(ranges::to<decltype(Expecting)>(Expecting_)),
        Actual(ranges::to<decltype(Actual)>(Actual_)) {}

  void signal() override { throw *this; }
  auto getExpectingTypes() const { return ranges::views::all(Expecting); }
  auto getActualTypes() const { return ranges::views::all(Actual); }
  bool getEpsilon() const { return Epsilon; }
};

// clang-format off
#define SABLE_VALIDATION_MALFORMED_ERROR_KINDS                                 \
  X(MISSING_CONTEXT_RETURN    , "return values have not been set yet"      )   \
  X(MALFORMED_FUNCTION_TYPE   , "malformed function type"                  )   \
  X(MALFORMED_VALUE_TYPE      , "malformed value type"                     )   \
  X(MALFORMED_MEMORY_TYPE     , "malformed memory type"                    )   \
  X(MALFORMED_TABLE_TYPE      , "malformed table type"                     )   \
  X(MALFORMED_GLOBAL_TYPE     , "malformed global type"                    )   \
  X(MALFORMED_LOCAL_VALUE_TYPE, "local has malformed value type"           )   \
  X(TYPE_INDEX_OUT_OF_BOUND   , "type index out-of-bound"                  )   \
  X(LABEL_INDEX_OUT_OF_BOUND  , "label index out-of-bound"                 )   \
  X(FUNC_INDEX_OUT_OF_BOUND   , "function index out-of-bound"              )   \
  X(TABLE_INDEX_OUT_OF_BOUND  , "table index out-of-bound"                 )   \
  X(MEM_INDEX_OUT_OF_BOUND    , "memory index out-of-bound"                )   \
  X(LOCAL_INDEX_OUT_OF_BOUND  , "local index out-of-bound"                 )   \
  X(GLOBAL_INDEX_OUT_OF_BOUND , "global index out-of-bound"                )   \
  X(INVALID_BRANCH_TABLE      , "label types in branch table do not argree")   \
  X(INVALID_ALIGN             , "malformed alignment hint"                 )   \
  X(GLOBAL_MUST_BE_MUT        , "global is not mutable"                    )   \
  X(NON_CONST_EXPRESSION      , "expression is not constant"               )   \
  X(INVALID_START_FUNC_TYPE   , "start function has mismatched type"       )   \
  X(MORE_THAN_ONE_TABLE       , "at most one table is allowed"             )   \
  X(MORE_THAN_ONE_MEMORY      , "at most one memory is allowed"            )   \
  X(NON_UNIQUE_EXPORT_NAME    , "export name is not unqiue"                )
// clang-format on

enum class MalformedErrorKind {
#define X(Name, Message) Name,
  SABLE_VALIDATION_MALFORMED_ERROR_KINDS
#undef X
};

class MalformedError : public ValidationError {
  MalformedErrorKind Kind;

public:
  template <ranges::input_range T>
  MalformedError(
      EntitySiteKind ESiteKind_, std::size_t ESiteIndex_, T const &ISites_,
      MalformedErrorKind Kind_)
      : ValidationError(ESiteKind_, ESiteIndex_, ISites_), Kind(Kind_) {}
  void signal() override { throw *this; }
  MalformedErrorKind getKind() const { return Kind; }
};

class TraceCollector {
  EntitySiteKind ESiteKind;
  std::size_t ESiteIndex;
  std::vector<InstructionPtr const *> ISites;

  template <typename T, typename... ArgTypes>
  std::unique_ptr<ValidationError> BuildErrorImpl(ArgTypes &&...Args) {
    return std::make_unique<T>(
        ESiteKind, ESiteIndex, ISites, std::forward<ArgTypes>(Args)...);
  }

  void enterEntity(EntitySiteKind ESiteKind_, std::size_t Index_);

public:
  template <ranges::input_range T, ranges::input_range U>
  std::unique_ptr<ValidationError>
  BuildError(bool Epsilon, T const &Expecting, U const &Actual) {
    return BuildErrorImpl<TypeError>(Epsilon, Expecting, Actual);
  }

  std::unique_ptr<ValidationError> BuildError(MalformedErrorKind Kind) {
    return BuildErrorImpl<MalformedError>(Kind);
  }

  void enterType(std::size_t Index);
  void enterFunction(std::size_t Index);
  void enterTable(std::size_t Index);
  void enterMemory(std::size_t Index);
  void enterGlobal(std::size_t Index);
  void enterElement(std::size_t Index);
  void enterData(std::size_t Index);
  void enterImport(std::size_t Index);
  void enterExport(std::size_t Index);
  void enterStart();

  void pushInstSite(InstructionPtr const &InstPtr);
  void popInstSite();
};

std::unique_ptr<ValidationError> validate(Module const &M);
} // namespace bytecode::validation

////////////////////////////////// Formatters //////////////////////////////////
namespace fmt {
template <> struct formatter<bytecode::validation::OperandStackElement> {
  using value_type = bytecode::validation::OperandStackElement;
  template <typename C> auto parse(C &&CTX) { return CTX.begin(); }
  template <typename C> auto format(value_type const &Type, C &&CTX) {
    using namespace bytecode::validation;
    using namespace bytecode;
    if (std::holds_alternative<ValueType>(Type)) {
      return fmt::format_to(CTX.out(), "{}", std::get<ValueType>(Type));
    }
    if (std::holds_alternative<TypeVariable>(Type)) {
      auto ID = std::get<TypeVariable>(Type).getID();
      return fmt::format_to(CTX.out(), "t{}", ID);
    }
    SABLE_UNREACHABLE();
  }
};

template <> struct formatter<bytecode::validation::TypeError> {
  using value_type = bytecode::validation::TypeError;
  template <typename C> auto parse(C &&CTX) { return CTX.begin(); }
  template <typename C> auto format(value_type const &Error, C &&CTX) {
    char const *Separator = "";
    auto Out = CTX.out();
    Out = fmt::format_to(Out, "type error, expecting [");
    for (auto const &ExpectType : Error.getExpectingTypes()) {
      Out = fmt::format_to(Out, "{}{}", Separator, ExpectType);
      Separator = ", ";
    }
    Separator = "";
    Out = fmt::format_to(Out, "], but [");
    for (auto const &ActualType : Error.getActualTypes()) {
      Out = fmt::format_to(Out, "{}{}", Separator, ActualType);
      Separator = ", ";
    }
    Out = fmt::format_to(Out, "] found.");
    return Out;
  }
};

template <> struct formatter<bytecode::validation::MalformedError> {
  using value_type = bytecode::validation::MalformedError;
  using MalformedErrorKind = bytecode::validation::MalformedErrorKind;
  template <typename C> auto parse(C &&CTX) { return CTX.begin(); }
  template <typename C> auto format(value_type const &Error, C &&CTX) {
    switch (Error.getKind()) {
#define X(Name, Message)                                                       \
  case MalformedErrorKind::Name: return fmt::format_to(CTX.out(), Message);
      SABLE_VALIDATION_MALFORMED_ERROR_KINDS
#undef X
    }
  }
};

#undef SABLE_VALIDATION_MALFORMED_ERROR_KINDS
} // namespace fmt

#endif
