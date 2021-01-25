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

enum class MalformedErrorKind {
  MISSING_CONTEXT_RETURN,
  MALFORMED_FUNCTION_TYPE,
  MALFORMED_VALUE_TYPE,
  MALFORMED_MEMORY_TYPE,
  MALFORMED_TABLE_TYPE,
  MALFORMED_GLOBAL_TYPE,
  TYPE_INDEX_OUT_OF_BOUND,
  LABEL_INDEX_OUT_OF_BOUND,
  FUNC_INDEX_OUT_OF_BOUND,
  TABLE_INDEX_OUT_OF_BOUND,
  MEM_INDEX_OUT_OF_BOUND,
  LOCAL_INDEX_OUT_OF_BOUND,
  GLOBAL_INDEX_OUT_OF_BOUND,
  INVALID_BRANCH_TABLE,
  INVALID_ALIGN,
  GLOBAL_MUST_BE_MUT
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

  void pushInstSite(InstructionPtr const &InstPtr);
  void popInstSite();
};

std::unique_ptr<ValidationError> validate(Module const &M);
} // namespace bytecode::validation

#endif
