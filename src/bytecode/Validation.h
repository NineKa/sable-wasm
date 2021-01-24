#ifndef SABLE_INCLUDE_GUARD_BYTECODE_VALIDATION
#define SABLE_INCLUDE_GUARD_BYTECODE_VALIDATION

#include "Instruction.h"
#include "Module.h"
#include "Type.h"

#include <variant>
#include <vector>

namespace bytecode::validation {
bool validateType(ValueType const &Type);
bool validateType(TableType const &Type);
bool validateType(MemoryType const &Type);
bool validateType(FunctionType const &Type);

class TypeVariable {
  std::size_t ID;

public:
  explicit TypeVariable(std::size_t ID_) : ID(ID_) {}
  std::size_t id() const { return ID; }
  bool operator==(TypeVariable const &) const = default;
  auto operator<=>(TypeVariable const &) const = default;
};

using OperandStackElement = std::variant<ValueType, TypeVariable>;

class ValidationContext;
class ValidationError : std::runtime_error {
  friend class ValidationContext;
  bytecode::Module const *ModuleSite;
  void const *EntitySite; // type erased entity site pointer
  std::vector<InstructionPtr const *> Site;

public:
  ValidationError() : std::runtime_error("validation error") {}
  using std::runtime_error::runtime_error;
  virtual void signal() = 0;

  InstructionPtr const *getLatestInstSite() const { return Site.back(); }
};

class TypeError : public ValidationError {
  bool Epsilon;
  std::vector<OperandStackElement> Expecting;
  std::vector<OperandStackElement> Actual;

public:
  template <ranges::input_range T, ranges::input_range U>
  TypeError(bool Epsilon_, T const &Expecting_, U const &Actual_)
      : Epsilon(Epsilon_),
        Expecting(ranges::to<decltype(Expecting)>(Expecting_)),
        Actual(ranges::to<decltype(Actual)>(Actual_)) {}

  void signal() override { throw *this; }

  auto expecting() const { return ranges::views::all(Expecting); }
  auto actual() const { return ranges::views::all(Actual); }
  bool epsilon() const { return Epsilon; }
};

enum class MalformedErrorKind {
  MISSING_CONTEXT_RETURN,
  MALFORMED_FUNCTION_TYPE,
  MALFORMED_VALUE_TYPE,
  MALFORMED_MEMORY_TYPE,
  MALFORMED_TABLE_TYPE,
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
  MalformedError(MalformedErrorKind Kind_) : Kind(Kind_) {}
  MalformedErrorKind kind() const { return Kind; }

  void signal() override { throw *this; }
};

bool validate(Module const *M);
void validateThrow(Module const *M);
} // namespace bytecode::validation

////////////////////////////////// Formatters //////////////////////////////////
namespace fmt {
template <> struct formatter<bytecode::validation::OperandStackElement> {
  using T = bytecode::validation::OperandStackElement;
  template <typename C> auto parse(C &&CTX) { return CTX.begin(); }
  template <typename C> auto format(T const &Element, C &&CTX) {
    using ValueType = bytecode::ValueType;
    using TypeVariable = bytecode::validation::TypeVariable;
    if (std::holds_alternative<ValueType>(Element)) {
      return fmt::format_to(CTX.out(), "{}", std::get<ValueType>(Element));
    } else if (std::holds_alternative<TypeVariable>(Element)) {
      auto const &TV = std::get<TypeVariable>(Element);
      return fmt::format_to(CTX.out(), "t{}", TV.id());
    } else
      SABLE_UNREACHABLE();
  }
};
} // namespace fmt

#endif
