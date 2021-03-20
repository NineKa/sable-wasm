#include "IsWellformedPass.h"
#include "../../bytecode/Validation.h"

#include <iterator>
#include <memory>

#include <range/v3/algorithm/copy.hpp>

namespace mir::passes {
// clang-format off
bool IsWellformedModulePass::has(mir::Global const &Global) const 
{ return AvailableNodes.contains(std::addressof(Global)); }
bool IsWellformedModulePass::has(mir::Memory const &Memory) const 
{ return AvailableNodes.contains(std::addressof(Memory)); }
bool IsWellformedModulePass::has(mir::Table const &Table) const 
{ return AvailableNodes.contains(std::addressof(Table)); }
bool IsWellformedModulePass::has(mir::Function const &Function) const 
{ return AvailableNodes.contains(std::addressof(Function)); }
bool IsWellformedModulePass::has(mir::DataSegment const &Data) const
{ return AvailableNodes.contains(std::addressof(Data)); }
bool IsWellformedModulePass::has(mir::ElementSegment const &Element) const
{ return AvailableNodes.contains(std::addressof(Element)); }
// clang-format on

IsWellformedPassResult::IsWellformedPassResult(
    std::span<std::pair<ASTNode *, ErrorKind> const> Sites_)
    : Sites(Sites_) {}

struct IsWellformedModulePass::CheckInitializeExprVisitor :
    InitExprVisitorBase<CheckInitializeExprVisitor, void, false> {
  using ErrorKind = IsWellformedPassResult::ErrorKind;
  IsWellformedModulePass &ModulePass;

public:
  CheckInitializeExprVisitor(IsWellformedModulePass &ModulePass_)
      : ModulePass(ModulePass_) {}
  void operator()(initializer::Constant *) { return; }
  void operator()(initializer::GlobalGet *InitInst) {
    auto const *Target = InitInst->getGlobalValue();
    if (Target == nullptr)
      ModulePass.Sites.emplace_back(InitInst, ErrorKind::NullOperand);
    if ((Target != nullptr) && !ModulePass.has(*Target))
      ModulePass.Sites.emplace_back(InitInst, ErrorKind::UnavailableOperand);
  }
};

void IsWellformedModulePass::checkInitializeExpr(InitializerExpr *Expr) {
  CheckInitializeExprVisitor Visitor(*this);
  Visitor.visit(Expr);
}

namespace {
namespace detail {
template <typename T>
class AddrEmplaceIterator :
    public std::iterator<std::output_iterator_tag, typename T::value_type> {
  T *Container;

public:
  using container_type = T;
  AddrEmplaceIterator() = default;
  AddrEmplaceIterator(AddrEmplaceIterator const &) = default;
  AddrEmplaceIterator(AddrEmplaceIterator &&) noexcept = default;
  AddrEmplaceIterator &operator=(AddrEmplaceIterator const &) = default;
  AddrEmplaceIterator &operator=(AddrEmplaceIterator &&) noexcept = default;
  ~AddrEmplaceIterator() noexcept = default;
  explicit AddrEmplaceIterator(T &Container_)
      : Container(std::addressof(Container_)) {}

  AddrEmplaceIterator &operator*() { return *this; }
  AddrEmplaceIterator &operator++() { return *this; }
  AddrEmplaceIterator &operator++(int) { return *this; }
  template <typename U> AddrEmplaceIterator &operator=(U &&Arg) {
    Container->emplace(std::addressof(Arg));
    return *this;
  }
};
} // namespace detail
} // namespace

PassResult IsWellformedModulePass::run(mir::Module &Module) {
  detail::AddrEmplaceIterator Iterator(AvailableNodes);
  ranges::copy(Module.getMemories(), Iterator);
  ranges::copy(Module.getTables(), Iterator);
  ranges::copy(Module.getGlobals(), Iterator);
  ranges::copy(Module.getFunctions(), Iterator);
  ranges::copy(Module.getData(), Iterator);
  ranges::copy(Module.getElements(), Iterator);

  for (auto &Memory : Module.getMemories()) {
    auto *MemoryPtr = std::addressof(Memory);
    for (auto const *Initializer : Memory.getInitializers()) {
      if (Initializer == nullptr)
        Sites.emplace_back(MemoryPtr, ErrorKind::NullOperand);
      if ((Initializer != nullptr) && !has(*Initializer))
        Sites.emplace_back(MemoryPtr, ErrorKind::UnavailableOperand);
      if (bytecode::validation::validate(Memory.getType()))
        Sites.emplace_back(MemoryPtr, ErrorKind::InvalidType);
    }
  }
  for (auto &Table : Module.getTables()) {
    auto *TablePtr = std::addressof(Table);
    for (auto const *Initializer : Table.getInitializers()) {
      if (Initializer == nullptr)
        Sites.emplace_back(TablePtr, ErrorKind::NullOperand);
      if ((Initializer != nullptr) && !has(*Initializer))
        Sites.emplace_back(TablePtr, ErrorKind::UnavailableOperand);
      if (bytecode::validation::validate(Table.getType()))
        Sites.emplace_back(TablePtr, ErrorKind::InvalidType);
    }
  }
  for (auto &Global : Module.getGlobals()) {
    auto *GlobalPtr = std::addressof(Global);
    if (Global.isImported() && Global.hasInitializer())
      Sites.emplace_back(GlobalPtr, ErrorKind::InvalidImport);
    if (Global.isExported() &&
        !(Global.isImported() || Global.hasInitializer()))
      Sites.emplace_back(GlobalPtr, ErrorKind::InvalidExport);
    if (Global.hasInitializer()) checkInitializeExpr(Global.getInitializer());
    if (bytecode::validation::validate(Global.getType()))
      Sites.emplace_back(GlobalPtr, ErrorKind::InvalidType);
  }
  for (auto &Function : Module.getFunctions()) {
    auto *FunctionPtr = std::addressof(Function);
    if (Function.isImported() && Function.hasBody())
      Sites.emplace_back(FunctionPtr, ErrorKind::InvalidImport);
    if (Function.isExported() && !(Function.isImported() || Function.hasBody()))
      Sites.emplace_back(FunctionPtr, ErrorKind::InvalidExport);
    if (bytecode::validation::validate(Function.getType()))
      Sites.emplace_back(FunctionPtr, ErrorKind::InvalidType);
    IsWellformedFunctionPass FunctionPass(*this);
    FunctionPass.run(Function);
    ranges::copy(FunctionPass.getResult(), std::back_inserter(Sites));
  }

  return PassResult::Converged;
}

IsWellformedModulePass::AnalysisResult
IsWellformedModulePass::getResult() const {
  return IsWellformedPassResult(Sites);
}

} // namespace mir::passes
