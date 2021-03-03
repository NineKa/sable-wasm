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
  prepareEntities(Module_.getMemories());
  prepareEntities(Module_.getTables());
  prepareEntities(Module_.getGlobals());
  prepareEntities(Module_.getFunctions());
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
      Names.emplace(std::addressof(Instruction), UnnamedLocalCounter);
      UnnamedLocalCounter = UnnamedLocalCounter + 1;
    }
  }
}

template <std::output_iterator<char> Iterator>
Iterator LocalNameWriter::write(Iterator Out, Local const &Local_) const {
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
char const *
MIRIteratorWriter<Iterator>::toString(instructions::IntUnaryOperator Op) {
  using UnaryOp = instructions::IntUnaryOperator;
  // clang-format off
  switch (Op) {
  case UnaryOp::Eqz      : return "int.eqz";
  case UnaryOp::Clz      : return "int.clz";
  case UnaryOp::Ctz      : return "int.ctz";
  case UnaryOp::Popcnt   : return "int.popcnt";
  default:SABLE_UNREACHABLE();
  }
  // clang-format on
}

template <std::output_iterator<char> Iterator>
char const *
MIRIteratorWriter<Iterator>::toString(instructions::IntBinaryOperator Op) {
  using BinaryOp = instructions::IntBinaryOperator;
  // clang-format off
  switch (Op) {
  case BinaryOp::Eq      : return "int.eq";
  case BinaryOp::Ne      : return "int.ne";
  case BinaryOp::LtS     : return "int.lt.s";
  case BinaryOp::LtU     : return "int.lt.u";
  case BinaryOp::GtS     : return "int.gt.s";
  case BinaryOp::GtU     : return "int.gt.u";
  case BinaryOp::LeS     : return "int.le.s";
  case BinaryOp::LeU     : return "int.le.u";
  case BinaryOp::GeS     : return "int.ge.s";
  case BinaryOp::GeU     : return "int.ge.u";
  case BinaryOp::Add     : return "int.add";
  case BinaryOp::Sub     : return "int.sub";
  case BinaryOp::Mul     : return "int.mul";
  case BinaryOp::DivS    : return "int.div.s";
  case BinaryOp::DivU    : return "int.div.u";
  case BinaryOp::RemS    : return "int.rem.s";
  case BinaryOp::RemU    : return "int.rem.u";
  case BinaryOp::And     : return "int.and";
  case BinaryOp::Or      : return "int.or";
  case BinaryOp::Xor     : return "int.xor";
  case BinaryOp::Shl     : return "int.shl";
  case BinaryOp::ShrS    : return "int.shr.s";
  case BinaryOp::ShrU    : return "int.shr.u";
  case BinaryOp::Rotl    : return "int.rotl";
  case BinaryOp::Rotr    : return "int.rotr";
  default: SABLE_UNREACHABLE();
  }
  // clang-format on
}

template <std::output_iterator<char> Iterator>
char const *
MIRIteratorWriter<Iterator>::toString(instructions::FPUnaryOperator Op) {
  using UnaryOp = instructions::FPUnaryOperator;
  // clang-format off
  switch (Op) {
  case UnaryOp::Abs      : return "fp.abs";
  case UnaryOp::Neg      : return "fp.neg";
  case UnaryOp::Ceil     : return "fp.ceil";
  case UnaryOp::Floor    : return "fp.floor";
  case UnaryOp::Trunc    : return "fp.truncate";
  case UnaryOp::Nearest  : return "fp.nearest";
  case UnaryOp::Sqrt     : return "fp.sqrt";
  default: SABLE_UNREACHABLE();
  }
  // clang-format on
}

template <std::output_iterator<char> Iterator>
char const *
MIRIteratorWriter<Iterator>::toString(instructions::FPBinaryOperator Op) {
  using BinaryOp = instructions::FPBinaryOperator;
  // clang-format off
  switch (Op) {
  case BinaryOp::Eq      : return "fp.eq";
  case BinaryOp::Ne      : return "fp.ne";
  case BinaryOp::Lt      : return "fp.lt";
  case BinaryOp::Gt      : return "fp.gt";
  case BinaryOp::Le      : return "fp.le";
  case BinaryOp::Ge      : return "fp.ge";
  case BinaryOp::Add     : return "fp.add";
  case BinaryOp::Sub     : return "fp.sub";
  case BinaryOp::Mul     : return "fp.mul";
  case BinaryOp::Div     : return "fp.div";
  case BinaryOp::Min     : return "fp.min";
  case BinaryOp::Max     : return "fp.max";
  case BinaryOp::CopySign: return "fp.copysign";
  default: SABLE_UNREACHABLE();
  }
  // clang-format on
}

template <std::output_iterator<char> Iterator>
char const *MIRIteratorWriter<Iterator>::toString(instructions::CastMode Mode) {
  using MKind = instructions::CastMode;
  // clang-format off
  switch (Mode) {
  case MKind::Conversion           : return "cast";
  case MKind::ConversionSigned     : return "cast.s";
  case MKind::ConversionUnsigned   : return "cast.u";
  case MKind::SatConversionSigned  : return "cast.sat.s";
  case MKind::SatConversionUnsigned: return "cast.sat.u";
  case MKind::Reinterpret          : return "cast.bit";
  default: SABLE_UNREACHABLE();
  }
  // clang-format on
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
Iterator dumpImportInfo(Iterator Out, detail::ImportableEntity const &Entity) {
  assert(Entity.isImported());
  MIRIteratorWriter Writer(Out);
  auto ModuleName = Entity.getImportModuleName();
  auto EntityName = Entity.getImportEntityName();
  Writer << "@import " << ModuleName << "::" << EntityName
         << Writer.linebreak();
  return Writer.iterator();
}

template <std::output_iterator<char> Iterator>
Iterator dumpExportInfo(Iterator Out, detail::ExportableEntity const &Entity) {
  assert(Entity.isExported());
  MIRIteratorWriter Writer(Out);
  auto EntityName = Entity.getExportName();
  Writer << "@export " << EntityName << Writer.linebreak();
  return Writer.iterator();
}

namespace detail {
template <std::output_iterator<char> Iterator>
class WriterInstructionVisitor :
    public InstVisitorBase<WriterInstructionVisitor<Iterator>, Iterator> {
  MIRIteratorWriter<Iterator> Writer;

public:
  WriterInstructionVisitor(
      Iterator Out, EntityNameWriter const &ENameWriter_,
      LocalNameWriter const &LNameWriter_)
      : Writer(Out, ENameWriter_, LNameWriter_) {}

  Iterator operator()(instructions::Unreachable const *) {
    return (Writer << "unreachable").iterator();
  }

  Iterator operator()(instructions::Branch const *Inst) {
    if (Inst->isConditional()) {
      Writer << "br.cond " << Inst->getCondition() << ' ' << Inst->getTarget();
    } else {
      assert(Inst->isUnconditional());
      Writer << "br " << Inst->getTarget();
    }
    return Writer.iterator();
  }

  Iterator operator()(instructions::BranchTable const *Inst) {
    Writer << "br.table " << Inst->getDefaultTarget();
    auto Targets = Inst->getTargets();
    for (std::size_t I = 0; I < Targets.size(); ++I) {
      Writer << ' ' << I << ':' << Targets[I];
    }
    return Writer.iterator();
  }

  Iterator operator()(instructions::Return const *Inst) {
    Writer << "ret";
    if (Inst->hasReturnValue()) { Writer << ' ' << Inst->getOperand(); }
    return Writer.iterator();
  }

  Iterator operator()(instructions::Call const *Inst) {
    Writer << Inst << " = call " << Inst->getTarget() << '(';
    char const *Separator = "";
    for (auto const *Argument : Inst->getArguments()) {
      Writer << Separator << Argument;
      Separator = ", ";
    }
    Writer << ')';
    return Writer.iterator();
  }

  Iterator operator()(instructions::CallIndirect const *Inst) {
    Writer << Inst << " = call.indirect " << Inst->getIndirectTable() << ' '
           << Inst->getOperand() << " (";
    char const *Separator = "";
    for (auto const *Argument : Inst->getArguments()) {
      Writer << Separator << Argument;
      Separator = ", ";
    }
    Writer << ')';
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
    Writer << "local.set " << Inst->getTarget() << Inst->getOperand();
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
    case VKind::I32: Writer << Inst->getI32(); break;
    case VKind::I64: Writer << Inst->getI64(); break;
    case VKind::F32: Writer << Inst->getF32(); break;
    case VKind::F64: Writer << Inst->getF64(); break;
    default: SABLE_UNREACHABLE();
    }
    return Writer.iterator();
  }

  Iterator operator()(instructions::IntUnaryOp const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = " << Operator << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::IntBinaryOp const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LHS << ' ' << RHS;
    return Writer.iterator();
  }

  Iterator operator()(instructions::FPUnaryOp const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = " << Operator << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::FPBinaryOp const *Inst) {
    auto Operator = Inst->getOperator();
    auto const *LHS = Inst->getLHS();
    auto const *RHS = Inst->getRHS();
    Writer << Inst << " = " << Operator << ' ' << LHS << ' ' << RHS;
    return Writer.iterator();
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

  Iterator operator()(instructions::MemorySize const *Inst) {
    auto const *Memory = Inst->getLinearMemory();
    Writer << Inst << " = memory.size " << Memory;
    return Writer.iterator();
  }

  Iterator operator()(instructions::MemoryGrow const *Inst) {
    auto const *Memory = Inst->getLinearMemory();
    auto const *Operand = Inst->getGrowSize();
    Writer << Inst << " = memory.grow " << Memory << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::MemoryGuard const *Inst) {
    auto const *Memory = Inst->getLinearMemory();
    auto const *Address = Inst->getAddress();
    auto GuardSize = Inst->getGuardSize();
    Writer << "memory.guard " << Memory << ' ' << Address << ' ' << GuardSize;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Cast const *Inst) {
    auto Mode = Inst->getMode();
    auto Type = Inst->getType();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = " << Mode << ' ' << Type << ' ' << Operand;
    return Writer.iterator();
  }

  Iterator operator()(instructions::Extend const *Inst) {
    auto Width = Inst->getFromWidth();
    auto const *Operand = Inst->getOperand();
    Writer << Inst << " = extend." << Width << ' ' << Operand;
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
    Writer << Inst << " = phi";
    for (auto const *Argument : Inst->getArguments()) Writer << ' ' << Argument;
    return Writer.iterator();
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

template <std::output_iterator<char> Iterator>
Iterator
dump(Iterator Out, Memory const &Memory_, EntityNameWriter const &ENameWriter) {
  if (Memory_.isImported()) Out = dumpImportInfo(Out, Memory_);
  if (Memory_.isExported()) Out = dumpExportInfo(Out, Memory_);
  MIRIteratorWriter Writer(Out, ENameWriter);
  Writer << "memory " << Memory_ << " : " << Memory_.getType()
         << Writer.linebreak();
  return Writer.iterator();
}

template <std::output_iterator<char> Iterator>
Iterator
dump(Iterator Out, Table const &Table_, EntityNameWriter const &ENameWriter) {
  if (Table_.isImported()) Out = dumpImportInfo(Out, Table_);
  if (Table_.isExported()) Out = dumpExportInfo(Out, Table_);
  MIRIteratorWriter Writer(Out, ENameWriter);
  Writer << "table " << Table_ << " : " << Table_.getType()
         << Writer.linebreak();
  return Writer.iterator();
}

template <std::output_iterator<char> Iterator>
Iterator
dump(Iterator Out, Global const &Global_, EntityNameWriter const &ENameWriter) {
  if (Global_.isImported()) Out = dumpImportInfo(Out, Global_);
  if (Global_.isExported()) Out = dumpExportInfo(Out, Global_);
  MIRIteratorWriter Writer(Out, ENameWriter);
  Writer << "global " << Global_ << " : " << Global_.getType()
         << Writer.linebreak();
  return Writer.iterator();
}

template <std::output_iterator<char> Iterator>
Iterator dump(
    Iterator Out, Function const &Function_,
    EntityNameWriter const &ENameWriter) {
  if (Function_.isImported()) Out = dumpImportInfo(Out, Function_);
  if (Function_.isExported()) Out = dumpExportInfo(Out, Function_);

  if (!Function_.isImported()) {
    LocalNameWriter LNameWriter(Function_);
    MIRIteratorWriter Writer(Out, ENameWriter, LNameWriter);
    Writer << "function " << Function_ << " : " << Function_.getType()
           << Writer.linebreak();
    char const *Separator = "";
    Writer << '{';
    for (auto const &Local : Function_.getLocals()) {
      Writer << Separator << Local << ":" << Local.getType();
      Separator = ", ";
    }
    Writer << '}' << Writer.linebreak();
    for (auto const &BasicBlock : Function_.getBasicBlocks()) {
      Writer << BasicBlock << ":" << Writer.linebreak();
      for (auto const &Instruction : BasicBlock) {
        Writer << Writer.indent();
        auto OutIterator =
            dump(Writer.iterator(), Instruction, ENameWriter, LNameWriter);
        Writer.attach(OutIterator);
        Writer << Writer.linebreak();
      }
    }
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
  ranges::for_each(Module_.getMemories(), [&](Memory const &Memory_) {
    Out = dump(Out, Memory_, ENameWriter);
    Out = (Writer.attach(Out) << Writer.linebreak()).iterator();
  });
  ranges::for_each(Module_.getTables(), [&](Table const &Table_) {
    Out = dump(Out, Table_, ENameWriter);
    Out = (Writer.attach(Out) << Writer.linebreak()).iterator();
  });
  ranges::for_each(Module_.getGlobals(), [&](Global const &Global_) {
    Out = dump(Out, Global_, ENameWriter);
    Out = (Writer.attach(Out) << Writer.linebreak()).iterator();
  });
  ranges::for_each(Module_.getFunctions(), [&](Function const &Function_) {
    Out = dump(Out, Function_, ENameWriter);
    Out = (Writer.attach(Out) << Writer.linebreak()).iterator();
  });
  return Out;
}
} // namespace mir
