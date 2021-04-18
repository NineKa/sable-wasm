#include "Vector.h"

#include <range/v3/algorithm/for_each.hpp>

namespace mir {
inline char const *const EntityNameWriter::NULL_STR = "null";

template <ranges::input_range T>
void EntityNameWriter::prepareEntities(T const &Entities) {
  std::size_t UnnamedCounter = 0;
  for (auto const &Entity : Entities) {
    if (Entity.hasName()) continue;
    Names.emplace(std::addressof(Entity), UnnamedCounter);
    UnnamedCounter = UnnamedCounter + 1;
  }
}

template <std::output_iterator<char> Iterator>
Iterator EntityNameWriter::write(
    Iterator Out, char const *FmtStr, ASTNode const &Node) const {
  auto const *NodePtr = std::addressof(Node);
  if (Node.hasName()) {
    Out = fmt::format_to(Out, "%{}", Node.getName());
  } else if (!Names.contains(NodePtr)) {
    Out = fmt::format_to(Out, FmtStr, fmt::ptr(NodePtr));
  } else {
    auto UnnamedIndex = std::get<1>(*Names.find(NodePtr));
    Out = fmt::format_to(Out, FmtStr, UnnamedIndex);
  }
  return Out;
}

inline EntityNameWriter::EntityNameWriter(Module const &Module_) {
  prepareEntities(Module_.getMemories().asView());
  prepareEntities(Module_.getTables().asView());
  prepareEntities(Module_.getGlobals().asView());
  prepareEntities(Module_.getFunctions().asView());
}

template <std::output_iterator<char> Iterator>
Iterator EntityNameWriter::write(Iterator Out, Memory const &Memory_) const {
  return write(Out, "%memory:{}", Memory_);
}

template <std::output_iterator<char> Iterator>
Iterator EntityNameWriter::write(Iterator Out, Memory const *MemoryPtr) const {
  if (MemoryPtr == nullptr) { return fmt::format_to(Out, "{}", NULL_STR); }
  return write(Out, *MemoryPtr);
}

template <std::output_iterator<char> Iterator>
Iterator EntityNameWriter::write(Iterator Out, Table const &Table_) const {
  return write(Out, "%table:{}", Table_);
}

template <std::output_iterator<char> Iterator>
Iterator EntityNameWriter::write(Iterator Out, Table const *TablePtr) const {
  if (TablePtr == nullptr) { return fmt::format_to(Out, "{}", NULL_STR); }
  return write(Out, *TablePtr);
}

template <std::output_iterator<char> Iterator>
Iterator EntityNameWriter::write(Iterator Out, Global const &Global_) const {
  return write(Out, "%global:{}", Global_);
}

template <std::output_iterator<char> Iterator>
Iterator EntityNameWriter::write(Iterator Out, Global const *GlobalPtr) const {
  if (GlobalPtr == nullptr) { return fmt::format_to(Out, "{}", NULL_STR); }
  return write(Out, *GlobalPtr);
}

template <std::output_iterator<char> Iterator>
Iterator
EntityNameWriter::write(Iterator Out, Function const &Function_) const {
  return write(Out, "%function:{}", Function_);
}

template <std::output_iterator<char> Iterator>
Iterator
EntityNameWriter::write(Iterator Out, Function const *FunctionPtr) const {
  if (FunctionPtr == nullptr) { return fmt::format_to(Out, "{}", NULL_STR); }
  return write(Out, *FunctionPtr);
}

inline char const *const LocalNameWriter::NULL_STR = "null";

template <std::output_iterator<char> Iterator>
Iterator LocalNameWriter::write(
    Iterator Out, char const *FmtStr, ASTNode const &Node) const {
  auto const *NodePtr = std::addressof(Node);
  if (Node.hasName()) {
    Out = fmt::format_to(Out, "%{}", Node.getName());
  } else if (!Names.contains(NodePtr)) {
    Out = fmt::format_to(Out, FmtStr, fmt::ptr(NodePtr));
  } else {
    auto UnnamedIndex = std::get<1>(*Names.find(NodePtr));
    Out = fmt::format_to(Out, FmtStr, UnnamedIndex);
  }
  return Out;
}

namespace detail {
inline bool isVoidReturnInst(Instruction const &Inst) {
  if (is_a<instructions::Call>(std::addressof(Inst))) {
    auto const *Ptr = dyn_cast<instructions::Call>(std::addressof(Inst));
    if ((Ptr->getTarget() != nullptr) &&
        (Ptr->getTarget()->getType().isVoidResult()))
      return true;
  }
  if (is_a<instructions::CallIndirect>(std::addressof(Inst))) {
    auto const *Ptr =
        dyn_cast<instructions::CallIndirect>(std::addressof(Inst));
    if (Ptr->getExpectType().isVoidResult()) return true;
  }
  switch (Inst.getInstructionKind()) {
  case InstructionKind::Store:
  case InstructionKind::LocalSet:
  case InstructionKind::GlobalSet:
  case InstructionKind::MemoryGuard:
  case InstructionKind::Branch: return true;
  default: return false;
  }
}
} // namespace detail

inline LocalNameWriter::LocalNameWriter(Function const &Function_) {
  assert(!Function_.isImported());
  std::size_t UnnamedLocalCounter = 0;
  std::size_t UnnamedBasicBlockCounter = 0;
  for (auto const &Local : Function_.getLocals()) {
    if (Local.hasName()) continue;
    Names.emplace(std::addressof(Local), UnnamedLocalCounter);
    UnnamedLocalCounter = UnnamedLocalCounter + 1;
  }
  for (auto const &BasicBlock : Function_.getBasicBlocks()) {
    if (!BasicBlock.hasName()) {
      Names.emplace(std::addressof(BasicBlock), UnnamedBasicBlockCounter);
      UnnamedBasicBlockCounter = UnnamedBasicBlockCounter + 1;
    }
    for (auto const &Instruction : BasicBlock) {
      if (Instruction.hasName()) continue;
      if (detail::isVoidReturnInst(Instruction)) continue;
      Names.emplace(std::addressof(Instruction), UnnamedLocalCounter);
      UnnamedLocalCounter = UnnamedLocalCounter + 1;
    }
  }
}

template <std::output_iterator<char> Iterator>
Iterator LocalNameWriter::write(Iterator Out, Local const &Local_) const {
  if (Local_.isParameter()) return write(Out, "(arg)%{}", Local_);
  return write(Out, "%{}", Local_);
}

template <std::output_iterator<char> Iterator>
Iterator LocalNameWriter::write(Iterator Out, Local const *LocalPtr) const {
  if (LocalPtr == nullptr) { return fmt::format_to(Out, "{}", NULL_STR); }
  return write(Out, *LocalPtr);
}

template <std::output_iterator<char> Iterator>
Iterator
LocalNameWriter::write(Iterator Out, BasicBlock const &BasicBlock_) const {
  return write(Out, "%BB:{}", BasicBlock_);
}

template <std::output_iterator<char> Iterator>
Iterator
LocalNameWriter::write(Iterator Out, BasicBlock const *BasicBlockPtr) const {
  if (BasicBlockPtr == nullptr) { return fmt::format_to(Out, "{}", NULL_STR); }
  return write(Out, *BasicBlockPtr);
}

template <std::output_iterator<char> Iterator>
Iterator
LocalNameWriter::write(Iterator Out, Instruction const &Instruction_) const {
  return write(Out, "%{}", Instruction_);
}

template <std::output_iterator<char> Iterator>
Iterator
LocalNameWriter::write(Iterator Out, Instruction const *InstructionPtr) const {
  if (InstructionPtr == nullptr) { return fmt::format_to(Out, "{}", NULL_STR); }
  return write(Out, *InstructionPtr);
}

template <std::output_iterator<char> Iterator>
char const *const MIRIteratorWriter<Iterator>::LINEBREAK_STR = "\n";

template <std::output_iterator<char> Iterator>
char const *const MIRIteratorWriter<Iterator>::INDENT_STR = "  ";

template <std::output_iterator<char> Iterator>
template <typename ArgType>
MIRIteratorWriter<Iterator> &
MIRIteratorWriter<Iterator>::forwardE(ArgType &&Arg) {
  assert(ENameWriter != nullptr);
  Out = ENameWriter->template write(Out, std::forward<ArgType>(Arg));
  return *this;
}

template <std::output_iterator<char> Iterator>
template <typename ArgType>
MIRIteratorWriter<Iterator> &
MIRIteratorWriter<Iterator>::forwardL(ArgType &&Arg) {
  assert(LNameWriter != nullptr);
  Out = LNameWriter->template write(Out, std::forward<ArgType>(Arg));
  return *this;
}

template <std::output_iterator<char> Iterator>
MIRIteratorWriter<Iterator>::MIRIteratorWriter(
    Iterator Out_, EntityNameWriter const &ENameWriter_)
    : Out(Out_), ENameWriter(std::addressof(ENameWriter_)) {}

template <std::output_iterator<char> Iterator>
MIRIteratorWriter<Iterator>::MIRIteratorWriter(
    Iterator Out_, EntityNameWriter const &ENameWriter_,
    LocalNameWriter const &LNameWriter_)
    : Out(Out_), ENameWriter(std::addressof(ENameWriter_)),
      LNameWriter(std::addressof(LNameWriter_)) {}

template <std::output_iterator<char> Iterator>
MIRIteratorWriter<Iterator> &
MIRIteratorWriter<Iterator>::operator<<(char Character) {
  *Out++ = Character;
  return *this;
}

template <std::output_iterator<char> Iterator>
MIRIteratorWriter<Iterator> &
MIRIteratorWriter<Iterator>::operator<<(ASTNode const *Node) {
  // clang-format off
  switch (Node->getASTNodeKind()) {
  case ASTNodeKind::Instruction: return *this << dyn_cast<Instruction>(Node);
  case ASTNodeKind::BasicBlock : return *this << dyn_cast<BasicBlock>(Node);
  case ASTNodeKind::Local      : return *this << dyn_cast<Local>(Node);
  case ASTNodeKind::Function   : return *this << dyn_cast<Function>(Node);
  case ASTNodeKind::Memory     : return *this << dyn_cast<Memory>(Node);
  case ASTNodeKind::Table      : return *this << dyn_cast<Table>(Node);
  case ASTNodeKind::Global     : return *this << dyn_cast<Global>(Node);
  default: utility::unreachable();
  }
  // clang-format on
}

template <std::output_iterator<char> Iterator>
MIRIteratorWriter<Iterator> &
MIRIteratorWriter<Iterator>::operator<<(std::string_view String) {
  Out = fmt::format_to(Out, "{}", String);
  return *this;
}

template <std::output_iterator<char> Iterator>
MIRIteratorWriter<Iterator> &
MIRIteratorWriter<Iterator>::operator<<(linebreak_ const &) {
  return operator<<(LINEBREAK_STR);
}

template <std::output_iterator<char> Iterator>
MIRIteratorWriter<Iterator> &
MIRIteratorWriter<Iterator>::operator<<(indent_ const &Indent_) {
  for (unsigned I = 0; I < Indent_.Level; ++I) operator<<(INDENT_STR);
  return *this;
}

template <std::output_iterator<char> Iterator>
Iterator dumpImportInfo(Iterator Out, ImportableEntity const &Entity) {
  assert(Entity.isImported());
  MIRIteratorWriter Writer(Out);
  auto ModuleName = Entity.getImportModuleName();
  auto EntityName = Entity.getImportEntityName();
  Writer << "@import " << ModuleName << "::" << EntityName
         << Writer.linebreak();
  return Writer.iterator();
}

template <std::output_iterator<char> Iterator>
Iterator dumpExportInfo(Iterator Out, ExportableEntity const &Entity) {
  assert(Entity.isExported());
  MIRIteratorWriter Writer(Out);
  auto EntityName = Entity.getExportName();
  Writer << "@export " << EntityName << Writer.linebreak();
  return Writer.iterator();
}

namespace detail {
template <std::output_iterator<char> Iterator>
// clang-format off
class WriterInstructionVisitor :
    public InstVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>,
    public mir::instructions::BranchVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>,
    public mir::instructions::CompareVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>,
    public mir::instructions::UnaryVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>,
    public mir::instructions::BinaryVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>,
    public mir::instructions::VectorSplatVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>,
    public mir::instructions::VectorExtractVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>,
    public mir::instructions::VectorInsertVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>
// clang-format on
{
  MIRIteratorWriter<Iterator> Writer;

public:
  using InstVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>::visit;

  WriterInstructionVisitor(
      Iterator Out, EntityNameWriter const &ENameWriter_,
      LocalNameWriter const &LNameWriter_)
      : Writer(Out, ENameWriter_, LNameWriter_) {}

  Iterator operator()(instructions::Unreachable const *) {
    return (Writer << "unreachable").iterator();
  }

  Iterator operator()(instructions::branch::Conditional const *Inst) {
    Writer << "br.cond " << Inst->getOperand() << ' ' << Inst->getTrue() << ' '
           << Inst->getFalse();
    return Writer.iterator();
  }

  Iterator operator()(instructions::branch::Unconditional const *Inst) {
    Writer << "br " << Inst->getTarget();
    return Writer.iterator();
  }

  Iterator operator()(instructions::branch::Switch const *Inst) {
    Writer << "br.table " << Inst->getDefaultTarget();
    for (std::size_t I = 0; I < Inst->getNumTargets(); ++I) {
      Writer << ' ' << I << ':' << Inst->getTarget(I);
    }
    return Writer.iterator();
  }

  Iterator operator()(instructions::Branch const *Inst) {
    using namespace mir::instructions;
    using BranchVisitor =
        BranchVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>;
    return BranchVisitor::visit(Inst);
  }

  Iterator operator()(instructions::Return const *Inst) {
    Writer << "ret";
    if (Inst->hasReturnValue()) { Writer << ' ' << Inst->getOperand(); }
    return Writer.iterator();
  }

  Iterator operator()(instructions::Call const *Inst) {
    if ((Inst->getTarget() != nullptr) &&
        (Inst->getTarget()->getType().isVoidResult())) {
      Writer << "call " << Inst->getTarget() << '(';
      char const *Separator = "";
      for (auto const *Argument : Inst->getArguments()) {
        Writer << Separator << Argument;
        Separator = ", ";
      }
      Writer << ')';
    } else {
      Writer << Inst << " = call " << Inst->getTarget() << '(';
      char const *Separator = "";
      for (auto const *Argument : Inst->getArguments()) {
        Writer << Separator << Argument;
        Separator = ", ";
      }
      Writer << ')';
    }
    return Writer.iterator();
  }

  Iterator operator()(instructions::CallIndirect const *Inst) {
    if (Inst->getExpectType().isVoidResult()) {
      Writer << "call.indirect " << Inst->getIndirectTable() << ' '
             << Inst->getOperand() << " (";
      char const *Separator = "";
      for (auto const *Argument : Inst->getArguments()) {
        Writer << Separator << Argument;
        Separator = ", ";
      }
      Writer << ')';
    } else {
      Writer << Inst << " = call.indirect " << Inst->getIndirectTable() << ' '
             << Inst->getOperand() << " (";
      char const *Separator = "";
      for (auto const *Argument : Inst->getArguments()) {
        Writer << Separator << Argument;
        Separator = ", ";
      }
      Writer << ')';
    }
    return Writer.iterator();
  }

  Iterator operator()(instructions::Select const *Inst) {
    auto const *Condition = Inst->getCondition();
    auto const *TrueValue = Inst->getTrue();
    auto const *FalseValue = Inst->getFalse();
    Writer << Inst << " = select " << Condition << ' ' << TrueValue << ' '
           << FalseValue;
    return Writer.iterator();
  }

  Iterator operator()(instructions::LocalGet const *Inst) {
    Writer << Inst << " = local.get " << Inst->getTarget();
    return Writer.iterator();
  }

  Iterator operator()(instructions::LocalSet const *Inst) {
    Writer << "local.set " << Inst->getTarget() << ' ' << Inst->getOperand();
    return Writer.iterator();
  }

  Iterator operator()(instructions::GlobalGet const *Inst) {
    Writer << Inst << " = global.get " << Inst->getTarget();
    return Writer.iterator();
  }

  Iterator operator()(instructions::GlobalSet const *Inst) {
    Writer << "global.set " << Inst->getTarget() << Inst->getOperand();
    return Writer.iterator();
  }

  Iterator operator()(instructions::Constant const *Inst) {
    auto ConstValueType = Inst->getValueType();
    Writer << Inst << " = const " << ConstValueType << " ";
    using VKind = bytecode::ValueTypeKind;
    switch (ConstValueType.getKind()) {
    case VKind::I32: Writer << Inst->asI32(); break;
    case VKind::I64: Writer << Inst->asI64(); break;
    case VKind::F32: Writer << Inst->asF32(); break;
    case VKind::F64: Writer << Inst->asF64(); break;
    default: utility::unreachable();
    }
    return Writer.iterator();
  }

  Iterator operator()(instructions::compare::IntCompare const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LHS << ' ' << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::compare::FPCompare const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LHS << ' ' << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::compare::SIMD128IntCompare const *Inst) {
    auto Operator = Inst->getOperator();
    auto LaneInfo = Inst->getLaneInfo();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LaneInfo << ' ' << LHS << ' '
           << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::compare::SIMD128FPCompare const *Inst) {
    auto Operator = Inst->getOperator();
    auto LaneInfo = Inst->getLaneInfo();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LaneInfo << ' ' << LHS << ' '
           << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Compare const *Inst) {
    using namespace mir::instructions;
    using CompareVisitor =
        CompareVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>;
    return CompareVisitor::visit(Inst);
  }

  Iterator operator()(instructions::unary::IntUnary const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = " << Operator << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::unary::FPUnary const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = " << Operator << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::unary::SIMD128Unary const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = " << Operator << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::unary::SIMD128IntUnary const *Inst) {
    auto Operator = Inst->getOperator();
    auto LaneInfo = Inst->getLaneInfo();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = " << Operator << ' ' << LaneInfo << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::unary::SIMD128FPUnary const *Inst) {
    auto Operator = Inst->getOperator();
    auto LaneInfo = Inst->getLaneInfo();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = " << Operator << ' ' << LaneInfo << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Unary const *Inst) {
    using namespace mir::instructions;
    using UnaryVisitor =
        UnaryVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>;
    return UnaryVisitor::visit(Inst);
  }

  Iterator operator()(instructions::binary::IntBinary const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LHS << ' ' << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::binary::FPBinary const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LHS << ' ' << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::binary::SIMD128Binary const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LHS << ' ' << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::binary::SIMD128IntBinary const *Inst) {
    auto Operator = Inst->getOperator();
    auto LaneInfo = Inst->getLaneInfo();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LaneInfo << ' ' << LHS << ' '
           << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::binary::SIMD128FPBinary const *Inst) {
    auto Operator = Inst->getOperator();
    auto LaneInfo = Inst->getLaneInfo();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LaneInfo << ' ' << LHS << ' '
           << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Binary const *Inst) {
    using namespace mir::instructions;
    using BinaryVisitor =
        BinaryVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>;
    return BinaryVisitor::visit(Inst);
  }

  Iterator operator()(instructions::Load const *Inst) {
    auto Width = Inst->getLoadWidth();
    auto Type = Inst->getType();
    auto const *Memory = Inst->getLinearMemory();
    auto const *Address = Inst->getAddress();
    Writer << Inst << " = load." << Width << ' ' << Type << ' ' << Memory << ' '
           << Address;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Store const *Inst) {
    auto Width = Inst->getStoreWidth();
    auto const *Memory = Inst->getLinearMemory();
    auto const *Address = Inst->getAddress();
    auto const *Operand = Inst->getOperand();
    Writer << "store." << Width << ' ' << Memory << ' ' << Address << ' '
           << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::MemoryGuard const *Inst) {
    auto const *Memory = Inst->getLinearMemory();
    auto const *Address = Inst->getAddress();
    auto GuardSize = Inst->getGuardSize();
    Writer << "memory.guard " << Memory << ' ' << Address << ' ' << GuardSize;
    return Writer.iterator();
  }

  Iterator operator()(instructions::MemoryGrow const *Inst) {
    auto const *Memory = Inst->getLinearMemory();
    auto const *Size = Inst->getSize();
    Writer << Inst << " = memory.grow " << Memory << ' ' << Size;
    return Writer.iterator();
  }

  Iterator operator()(instructions::MemorySize const *Inst) {
    auto const *Memory = Inst->getLinearMemory();
    Writer << Inst << " = memory.size " << Memory;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Cast const *Inst) {
    auto CastOpcode = Inst->getCastOpcode();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = cast " << CastOpcode << ' ' << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Pack const *Inst) {
    Writer << Inst << " = pack";
    for (auto const *Argument : Inst->getArguments()) Writer << ' ' << Argument;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Unpack const *Inst) {
    auto Index = Inst->getIndex();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = unpack " << Index << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Phi const *Inst) {
    Writer << Inst << " = phi " << Inst->getType();
    for (auto const &Candidate : Inst->getCandidates())
      Writer << " [" << std::get<0>(Candidate) << ", " << std::get<1>(Candidate)
             << ']';
    return Writer.iterator();
  }

  Iterator operator()(instructions::vector_splat::SIMD128IntSplat const *Inst) {
    Writer << Inst << " = v128.int.splat " << Inst->getLaneInfo() << ' '
           << Inst->getOperand();
    return Writer.iterator();
  }

  Iterator operator()(instructions::vector_splat::SIMD128FPSplat const *Inst) {
    Writer << Inst << " = v128.fp.splat " << Inst->getLaneInfo() << ' '
           << Inst->getOperand();
    return Writer.iterator();
  }

  Iterator operator()(instructions::VectorSplat const *Inst) {
    using namespace mir::instructions;
    using VectorSplatVisitor =
        VectorSplatVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>;
    return VectorSplatVisitor::visit(Inst);
  }

  Iterator
  operator()(instructions::vector_extract::SIMD128IntExtract const *Inst) {
    Writer << Inst << " = v128.int.extract " << Inst->getLaneInfo() << ' '
           << Inst->getLaneIndex() << ' ' << Inst->getOperand();
    return Writer.iterator();
  }

  Iterator
  operator()(instructions::vector_extract::SIMD128FPExtract const *Inst) {
    Writer << Inst << " = v128.fp.extract " << Inst->getLaneInfo() << ' '
           << Inst->getLaneIndex() << ' ' << Inst->getOperand();
    return Writer.iterator();
  }

  Iterator operator()(instructions::VectorExtract const *Inst) {
    using namespace mir::instructions;
    using VectorExtractVisitor =
        VectorExtractVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>;
    return VectorExtractVisitor::visit(Inst);
  }

  Iterator
  operator()(instructions::vector_insert::SIMD128IntInsert const *Inst) {
    Writer << Inst << " = v128.int.insert" << Inst->getLaneInfo() << ' '
           << Inst->getLaneIndex() << ' ' << Inst->getTargetVector() << ' '
           << Inst->getCandidateValue();
    return Writer.iterator();
  }

  Iterator
  operator()(instructions::vector_insert::SIMD128FPInsert const *Inst) {
    Writer << Inst << " = v128.fp.insert" << Inst->getLaneInfo() << ' '
           << Inst->getLaneIndex() << ' ' << Inst->getTargetVector() << ' '
           << Inst->getCandidateValue();
    return Writer.iterator();
  }

  Iterator operator()(instructions::VectorInsert const *Inst) {
    using namespace mir::instructions;
    using VectorInsertVisitor =
        VectorInsertVisitorBase<WriterInstructionVisitor<Iterator>, Iterator>;
    return VectorInsertVisitor::visit(Inst);
  }
};
} // namespace detail

template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, Instruction const &Instruction_,
    EntityNameWriter const &ENameWriter, LocalNameWriter const &LNameWriter) {
  detail::WriterInstructionVisitor Visitor(Out, ENameWriter, LNameWriter);
  return Visitor.visit(std::addressof(Instruction_));
}

namespace detail {
template <typename Iterator>
struct InitializerExprPrintVisitor :
    InitExprVisitorBase<InitializerExprPrintVisitor<Iterator>, Iterator> {
  MIRIteratorWriter<Iterator> Writer;
  InitializerExprPrintVisitor(Iterator Out, EntityNameWriter const &ENameWriter)
      : Writer(Out, ENameWriter) {}
  Iterator operator()(initializer::Constant const *InitExpr) {
    using VKind = bytecode::ValueTypeKind;
    switch (InitExpr->getValueType().getKind()) {
    case VKind::I32: return (Writer << "i32 " << InitExpr->asI32()).iterator();
    case VKind::I64: return (Writer << "i64 " << InitExpr->asI64()).iterator();
    case VKind::F32: return (Writer << "f32 " << InitExpr->asF32()).iterator();
    case VKind::F64: return (Writer << "f64 " << InitExpr->asF64()).iterator();
    default: utility::unreachable();
    }
  }
  Iterator operator()(initializer::GlobalGet const *InitExpr) {
    Writer << InitExpr->getGlobalValue();
    return Writer.iterator();
  }
};
} // namespace detail

template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, InitializerExpr const &InitializerExpr_,
    EntityNameWriter const &ENameWriter) {
  detail::InitializerExprPrintVisitor Visitor(Out, ENameWriter);
  return Visitor.visit(std::addressof(InitializerExpr_));
}

template <std::output_iterator<char> Iterator>
Iterator
dump(Iterator Out, Memory const &Memory_, EntityNameWriter const &ENameWriter) {
  if (Memory_.isImported()) Out = dumpImportInfo(Out, Memory_);
  if (Memory_.isExported()) Out = dumpExportInfo(Out, Memory_);
  MIRIteratorWriter Writer(Out, ENameWriter);
  Writer << "memory " << Memory_ << " : " << Memory_.getType();
  if (!Memory_.getInitializers().empty()) {
    Writer << " {" << Writer.linebreak();
    for (auto const *Initializer : Memory_.getInitializers()) {
      Writer << Writer.indent();
      Writer << "<" << Initializer->getSize() << " byte(s)...> at ";
      Writer.attach(
          dump(Writer.iterator(), *Initializer->getOffset(), ENameWriter));
      Writer << Writer.linebreak();
    }
    Writer << '}';
  }

  return (Writer << Writer.linebreak()).iterator();
}

template <std::output_iterator<char> Iterator>
Iterator
dump(Iterator Out, Table const &Table_, EntityNameWriter const &ENameWriter) {
  if (Table_.isImported()) Out = dumpImportInfo(Out, Table_);
  if (Table_.isExported()) Out = dumpExportInfo(Out, Table_);
  MIRIteratorWriter Writer(Out, ENameWriter);
  Writer << "table " << Table_ << " : " << Table_.getType();
  if (!Table_.getInitializers().empty()) {
    Writer << " {" << Writer.linebreak();
    for (auto const *Initializer : Table_.getInitializers()) {
      Writer << Writer.indent();
      Writer << "<" << Initializer->getSize() << " function(s)...> at ";
      Writer.attach(
          dump(Writer.iterator(), *Initializer->getOffset(), ENameWriter));
      Writer << Writer.linebreak();
    }
    Writer << '}';
  }

  return (Writer << Writer.linebreak()).iterator();
}

template <std::output_iterator<char> Iterator>
Iterator
dump(Iterator Out, Global const &Global_, EntityNameWriter const &ENameWriter) {
  if (Global_.isImported()) Out = dumpImportInfo(Out, Global_);
  if (Global_.isExported()) Out = dumpExportInfo(Out, Global_);
  MIRIteratorWriter Writer(Out, ENameWriter);
  Writer << "global " << Global_ << " : " << Global_.getType();
  if (Global_.isDefinition()) {
    Writer << " = ";
    Out = dump(Writer.iterator(), *Global_.getInitializer(), ENameWriter);
    Writer.attach(Out);
  }
  return (Writer << Writer.linebreak()).iterator();
}

template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, Function const &Function_,
    EntityNameWriter const &ENameWriter) {
  if (Function_.isImported()) Out = dumpImportInfo(Out, Function_);
  if (Function_.isExported()) Out = dumpExportInfo(Out, Function_);

  if (Function_.isDefinition()) {
    LocalNameWriter LNameWriter(Function_);
    MIRIteratorWriter Writer(Out, ENameWriter, LNameWriter);
    Writer << "function " << Function_ << " : " << Function_.getType() << " {"
           << Writer.linebreak();
    char const *Separator = "";
    Writer << '{';
    for (auto const &Local : Function_.getLocals()) {
      Writer << Separator << Local << ":" << Local.getType();
      Separator = ", ";
    }
    Writer << '}' << Writer.linebreak();
    for (auto const &BasicBlock : Function_.getBasicBlocks()) {
      auto Predecessors = BasicBlock.getInwardFlow();
      Writer << "#pred = {";
      auto const *BBSeparator = "";
      for (auto const *Predecessor : Predecessors) {
        Writer << BBSeparator << Predecessor;
        BBSeparator = ", ";
      }
      Writer << "}" << Writer.linebreak() << BasicBlock << ":"
             << Writer.linebreak();
      for (auto const &Instruction : BasicBlock) {
        Writer << Writer.indent();
        auto OutIterator =
            dump(Writer.iterator(), Instruction, ENameWriter, LNameWriter);
        Writer.attach(OutIterator);
        Writer << Writer.linebreak();
      }
      Writer << Writer.linebreak();
    }
    Writer << '}' << Writer.linebreak();
    return Writer.iterator();
  }
  MIRIteratorWriter Writer(Out, ENameWriter);
  Writer << "function " << Function_ << " : " << Function_.getType()
         << Writer.linebreak();
  return Writer.iterator();
}

template <std::output_iterator<char> Iterator>
Iterator dump(Iterator Out, Module const &Module_) {
  EntityNameWriter ENameWriter(Module_);
  MIRIteratorWriter<Iterator> Writer;
  for (auto const &Memory : Module_.getMemories().asView()) {
    Out = dump(Out, Memory, ENameWriter);
    Out = (Writer.attach(Out) << Writer.linebreak()).iterator();
  }
  for (auto const &Table : Module_.getTables().asView()) {
    Out = dump(Out, Table, ENameWriter);
    Out = (Writer.attach(Out) << Writer.linebreak()).iterator();
  }
  for (auto const &Global : Module_.getGlobals().asView()) {
    Out = dump(Out, Global, ENameWriter);
    Out = (Writer.attach(Out) << Writer.linebreak()).iterator();
  }
  for (auto const &Function : Module_.getFunctions().asView()) {
    Out = dump(Out, Function, ENameWriter);
    Out = (Writer.attach(Out) << Writer.linebreak()).iterator();
  }
  return Out;
}
} // namespace mir
