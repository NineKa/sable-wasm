#ifndef SABLE_INCLUDE_GUARD_MIR_PRINTER
#define SABLE_INCLUDE_GUARD_MIR_PRINTER

#include "BasicBlock.h"
#include "Binary.h"
#include "Branch.h"
#include "Compare.h"
#include "Instruction.h"
#include "Module.h"
#include "Unary.h"

#include <fmt/format.h>

#include <concepts>
#include <iterator>
#include <unordered_map>

namespace mir {

class EntityNameWriter {
  std::unordered_map<ASTNode const *, std::size_t> Names;
  static char const *const NULL_STR;

  template <ranges::input_range T> void prepareEntities(T const &Entities);
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, char const *FmtStr, ASTNode const &Node) const;

public:
  EntityNameWriter() = default;
  EntityNameWriter(Module const &Module_);

  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Memory const &Memory_) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Table const &Table_) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Global const &Global_) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Function const &Function_) const;

  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Memory const *MemoryPtr) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Table const *TablePtr) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Global const *GlobalPtr) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Function const *FunctionPtr) const;
};

class LocalNameWriter {
  std::unordered_map<ASTNode const *, std::size_t> Names;
  static char const *const NULL_STR;

  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, char const *FmtStr, ASTNode const &Node) const;

public:
  LocalNameWriter() = default;
  LocalNameWriter(Function const &Function_);

  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Local const &Local_) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, BasicBlock const &BasicBlock_) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Instruction const &Instruction_) const;

  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Local const *LocalPtr) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, BasicBlock const *BasicBlockPtr) const;
  template <std::output_iterator<char> Iterator>
  Iterator write(Iterator Out, Instruction const *InstructionPtr) const;
};

template <std::output_iterator<char> Iterator> class MIRIteratorWriter {
  Iterator Out;
  EntityNameWriter const *ENameWriter = nullptr;
  LocalNameWriter const *LNameWriter = nullptr;

  char const *toString(instructions::binary::IntBinaryOperator Operator);
  char const *toString(instructions::binary::FPBinaryOperator Operator);
  char const *toString(instructions::CastMode Mode);

  static const char *const LINEBREAK_STR;
  static const char *const INDENT_STR;

  template <typename ArgType> MIRIteratorWriter &forwardE(ArgType &&Arg);
  template <typename ArgType> MIRIteratorWriter &forwardL(ArgType &&Arg);

  struct linebreak_ {};
  struct indent_ {
    unsigned Level;
    explicit indent_(unsigned Level_) : Level(Level_) {}
  };

public:
  MIRIteratorWriter() = default;
  explicit MIRIteratorWriter(Iterator Out_) : Out(Out_) {}
  MIRIteratorWriter(Iterator Out_, EntityNameWriter const &ENameWriter_);
  MIRIteratorWriter(
      Iterator Out_, EntityNameWriter const &ENameWriter_,
      LocalNameWriter const &LNameWriter_);

  linebreak_ linebreak() const { return linebreak_{}; }
  indent_ indent(unsigned Level_ = 1) const { return indent_(Level_); }

  MIRIteratorWriter &operator<<(char Character);
  MIRIteratorWriter &operator<<(std::string_view String);
  MIRIteratorWriter &operator<<(linebreak_ const &);
  MIRIteratorWriter &operator<<(indent_ const &);

  // clang-format off
  MIRIteratorWriter &operator<<(instructions::compare::IntCompareOperator Op)
  { Out = fmt::format_to(Out, "{}", Op); return *this; }
  MIRIteratorWriter &operator<<(instructions::compare::FPCompareOperator Op)
  { Out = fmt::format_to(Out, "{}", Op); return *this; }
  MIRIteratorWriter &operator<<
  (instructions::compare::SIMD128IntCompareOperator Op)
  { Out = fmt::format_to(Out, "{}", Op); return *this; }
  MIRIteratorWriter &operator<<
  (instructions::compare::SIMD128FPCompareOperator Op)
  { Out = fmt::format_to(Out, "{}", Op); return *this; }
  MIRIteratorWriter &operator<<(SIMD128IntLaneInfo const &LaneInfo)
  { Out = fmt::format_to(Out, "{}", LaneInfo); return *this; }
  MIRIteratorWriter &operator<<(SIMD128FPLaneInfo const &LaneInfo)
  { Out = fmt::format_to(Out, "{}", LaneInfo); return *this; }


  MIRIteratorWriter &operator<<(instructions::unary::IntUnaryOperator Op)
  { Out = fmt::format_to(Out, "{}", Op); return *this; }
  MIRIteratorWriter &operator<<(instructions::unary::FPUnaryOperator Op)
  { Out = fmt::format_to(Out, "{}", Op); return *this; }
  MIRIteratorWriter &operator<<(instructions::unary::SIMD128UnaryOperator Op)
  { Out = fmt::format_to(Out, "{}", Op); return *this; }
  MIRIteratorWriter &operator<<(instructions::unary::SIMD128IntUnaryOperator Op)
  { Out = fmt::format_to(Out, "{}", Op); return *this; }
  MIRIteratorWriter &operator<<(instructions::unary::SIMD128FPUnaryOperator Op)
  { Out = fmt::format_to(Out, "{}", Op); return *this; }

  MIRIteratorWriter &operator<<(instructions::binary::IntBinaryOperator Op)
  { Out = fmt::format_to(Out, "{}", toString(Op)); return *this; }
  MIRIteratorWriter &operator<<(instructions::binary::FPBinaryOperator Op)
  { Out = fmt::format_to(Out, "{}", toString(Op)); return *this; }
  MIRIteratorWriter &operator<<(instructions::CastMode Mode)
  { Out = fmt::format_to(Out, "{}", toString(Mode)); return *this; }
  // clang-format on

  MIRIteratorWriter &operator<<(Memory const &X) { return forwardE(X); }
  MIRIteratorWriter &operator<<(Table const &X) { return forwardE(X); }
  MIRIteratorWriter &operator<<(Global const &X) { return forwardE(X); }
  MIRIteratorWriter &operator<<(Function const &X) { return forwardE(X); }
  MIRIteratorWriter &operator<<(Memory const *X) { return forwardE(X); }
  MIRIteratorWriter &operator<<(Table const *X) { return forwardE(X); }
  MIRIteratorWriter &operator<<(Global const *X) { return forwardE(X); }
  MIRIteratorWriter &operator<<(Function const *X) { return forwardE(X); }

  MIRIteratorWriter &operator<<(Local const &X) { return forwardL(X); }
  MIRIteratorWriter &operator<<(BasicBlock const &X) { return forwardL(X); }
  MIRIteratorWriter &operator<<(Instruction const &X) { return forwardL(X); }
  MIRIteratorWriter &operator<<(Local const *X) { return forwardL(X); }
  MIRIteratorWriter &operator<<(BasicBlock const *X) { return forwardL(X); }
  MIRIteratorWriter &operator<<(Instruction const *X) { return forwardL(X); }

  MIRIteratorWriter &operator<<(ASTNode const *Node);

  // clang-format off
  MIRIteratorWriter &operator<<(bytecode::ValueType const &Type)
  { Out = fmt::format_to(Out, "{}", Type); return *this; }
  MIRIteratorWriter &operator<<(bytecode::FunctionType const &Type)
  { Out = fmt::format_to(Out, "{}", Type); return *this; }
  MIRIteratorWriter &operator<<(bytecode::TableType const &Type)
  { Out = fmt::format_to(Out, "{}", Type); return *this; }
  MIRIteratorWriter &operator<<(bytecode::MemoryType const &Type)
  { Out = fmt::format_to(Out, "{}", Type); return *this; }
  MIRIteratorWriter &operator<<(bytecode::GlobalType const &Type)
  { Out = fmt::format_to(Out, "{}", Type); return *this; }


  template <std::integral T> MIRIteratorWriter &operator<<(T Value)
  { Out = fmt::format_to(Out, "{}", Value); return *this; }
  template <std::floating_point T> MIRIteratorWriter &operator<<(T Value)
  { Out = fmt::format_to(Out, "{}", Value); return *this; }
  // clang-format on

  Iterator iterator() const { return Out; }
  MIRIteratorWriter &attach(Iterator Out_) {
    Out = Out_;
    return *this;
  }
};

template <std::output_iterator<char> Iterator>
Iterator dumpImportInfo(Iterator Out, ImportableEntity const &Entity);
template <std::output_iterator<char> Iterator>
Iterator dumpExportInfo(Iterator Out, ExportableEntity const &Entity);

template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, Instruction const &Instruction_,
    EntityNameWriter const &ENameWriter = EntityNameWriter(),
    LocalNameWriter const &LNameWriter = LocalNameWriter());

template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, InitializerExpr const &ConstantExpr_,
    EntityNameWriter const &ENameWriter = EntityNameWriter());

template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, Memory const &Memory_,
    EntityNameWriter const &ENameWriter = EntityNameWriter());
template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, Table const &Table_,
    EntityNameWriter const &ENameWriter = EntityNameWriter());
template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, Global const &Global_,
    EntityNameWriter const &ENameWriter = EntityNameWriter());
template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, Function const &Function_,
    EntityNameWriter const &ENameWriter = EntityNameWriter());
template <std::output_iterator<char> Iterator>
Iterator dump(Iterator Out, Module const &Module_);

} // namespace mir

#include "MIRPrinter.tcc"

#endif
