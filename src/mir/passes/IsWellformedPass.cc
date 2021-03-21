#include "IsWellformedPass.h"
#include "../../bytecode/Validation.h"

#include <iterator>
#include <memory>

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/find_if.hpp>

namespace mir::passes {
// clang-format off
bool IsWellformedModulePass::has(mir::Global const &Global) const 
{ return AvailableNodes->contains(std::addressof(Global)); }
bool IsWellformedModulePass::has(mir::Memory const &Memory) const 
{ return AvailableNodes->contains(std::addressof(Memory)); }
bool IsWellformedModulePass::has(mir::Table const &Table) const 
{ return AvailableNodes->contains(std::addressof(Table)); }
bool IsWellformedModulePass::has(mir::Function const &Function) const 
{ return AvailableNodes->contains(std::addressof(Function)); }
bool IsWellformedModulePass::has(mir::DataSegment const &Data) const
{ return AvailableNodes->contains(std::addressof(Data)); }
bool IsWellformedModulePass::has(mir::ElementSegment const &Element) const
{ return AvailableNodes->contains(std::addressof(Element)); }
// clang-format on

IsWellformedPassResult::IsWellformedPassResult(
    std::shared_ptr<SiteVector> Sites_)
    : Sites(std::move(Sites_)) {}

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
    if (Target == nullptr) ModulePass.addSite(InitInst, ErrorKind::NullOperand);
    if ((Target != nullptr) && !ModulePass.has(*Target))
      ModulePass.addSite(InitInst, ErrorKind::UnavailableOperand);
  }
};

void IsWellformedModulePass::checkInitializeExpr(InitializerExpr *Expr) {
  CheckInitializeExprVisitor Visitor(*this);
  Visitor.visit(Expr);
}

void IsWellformedModulePass::addSite(ASTNode *Ptr, ErrorKind Reason) {
  auto SearchIter =
      ranges::find_if(*Sites, [=](std::pair<ASTNode *, ErrorKind> SitePair) {
        return std::get<0>(SitePair) == Ptr;
      });
  if (SearchIter == Sites->end()) Sites->emplace_back(Ptr, Reason);
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
void IsWellformedModulePass::prepare(mir::Module &Module_) {
  Module = std::addressof(Module_);
  Sites = std::make_shared<SiteVector>();
  AvailableNodes = std::make_unique<std::unordered_set<mir::ASTNode const *>>();
  detail::AddrEmplaceIterator Iterator(*AvailableNodes);
  ranges::copy(Module->getMemories(), Iterator);
  ranges::copy(Module->getTables(), Iterator);
  ranges::copy(Module->getGlobals(), Iterator);
  ranges::copy(Module->getFunctions(), Iterator);
  ranges::copy(Module->getData(), Iterator);
  ranges::copy(Module->getElements(), Iterator);
}

PassStatus IsWellformedModulePass::run() {
  for (auto &Memory : Module->getMemories()) {
    auto *MemoryPtr = std::addressof(Memory);
    for (auto const *Initializer : Memory.getInitializers()) {
      if (Initializer == nullptr) addSite(MemoryPtr, ErrorKind::NullOperand);
      if ((Initializer != nullptr) && !has(*Initializer))
        addSite(MemoryPtr, ErrorKind::UnavailableOperand);
      if (!bytecode::validation::validate(Memory.getType()))
        addSite(MemoryPtr, ErrorKind::InvalidType);
    }
  }

  for (auto &Table : Module->getTables()) {
    auto *TablePtr = std::addressof(Table);
    for (auto const *Initializer : Table.getInitializers()) {
      if (Initializer == nullptr) addSite(TablePtr, ErrorKind::NullOperand);
      if ((Initializer != nullptr) && !has(*Initializer))
        addSite(TablePtr, ErrorKind::UnavailableOperand);
      if (!bytecode::validation::validate(Table.getType()))
        addSite(TablePtr, ErrorKind::InvalidType);
    }
  }

  for (auto &Global : Module->getGlobals()) {
    auto *GlobalPtr = std::addressof(Global);
    if (Global.isImported() && Global.hasInitializer())
      addSite(GlobalPtr, ErrorKind::InvalidImport);
    if (Global.isExported() &&
        !(Global.isImported() || Global.hasInitializer()))
      addSite(GlobalPtr, ErrorKind::InvalidExport);
    if (Global.hasInitializer()) checkInitializeExpr(Global.getInitializer());
    if (!bytecode::validation::validate(Global.getType()))
      addSite(GlobalPtr, ErrorKind::InvalidType);
  }

  for (auto &Function : Module->getFunctions()) {
    auto *FunctionPtr = std::addressof(Function);
    if (Function.isImported() && Function.hasBody())
      addSite(FunctionPtr, ErrorKind::InvalidImport);
    if (Function.isExported() && !(Function.isImported() || Function.hasBody()))
      addSite(FunctionPtr, ErrorKind::InvalidExport);
    if (!bytecode::validation::validate(Function.getType()))
      addSite(FunctionPtr, ErrorKind::InvalidType);
    if (!Function.isImported()) {
      SimpleFunctionPassDriver<IsWellformedFunctionPass> Driver(*this);
      ranges::copy(Driver(Function), std::back_inserter(*Sites));
    }
  }

  for (auto &Data : Module->getData()) {
    auto *DataPtr = std::addressof(Data);
    if (DataPtr->getOffset() == nullptr) {
      addSite(DataPtr, ErrorKind::NullOperand);
    } else {
      checkInitializeExpr(Data.getOffset());
    }
  }

  for (auto &Element : Module->getElements()) {
    auto *ElementPtr = std::addressof(Element);
    if (ElementPtr->getOffset() == nullptr) {
      addSite(ElementPtr, ErrorKind::NullOperand);
    } else {
      checkInitializeExpr(Element.getOffset());
    }
    for (auto const *FunctionPtr : Element.getContent()) {
      if (FunctionPtr == nullptr) addSite(ElementPtr, ErrorKind::NullOperand);
      if ((FunctionPtr != nullptr) && !has(*FunctionPtr))
        addSite(ElementPtr, ErrorKind::UnavailableOperand);
    }
  }

  return PassStatus::Converged;
}

void IsWellformedModulePass::finalize() { AvailableNodes = nullptr; }

IsWellformedModulePass::AnalysisResult
IsWellformedModulePass::getResult() const {
  return IsWellformedPassResult(Sites);
}

void IsWellformedFunctionPass::addSite(ASTNode *Ptr, ErrorKind Reason) {
  auto SearchIter =
      ranges::find_if(*Sites, [=](std::pair<ASTNode *, ErrorKind> SitePair) {
        return std::get<0>(SitePair) == Ptr;
      });
  if (SearchIter == Sites->end()) Sites->emplace_back(Ptr, Reason);
}

IsWellformedFunctionPass::IsWellformedFunctionPass(
    IsWellformedModulePass const &ModulePass_)
    : ModulePass(std::addressof(ModulePass_)) {}

void IsWellformedFunctionPass::prepare(mir::Function &Function_) {
  Function = std::addressof(Function_);
  SimpleFunctionPassDriver<ReachingDefPass> Driver;
  using T = decltype(Driver)::AnalysisResult;
  ReachingDef = std::make_unique<T>(Driver(*Function));
  Sites = std::make_shared<SiteVector>();
}

PassStatus IsWellformedFunctionPass::run() { return PassStatus::Converged; }

void IsWellformedFunctionPass::finalize() {
  Function = nullptr;
  ReachingDef = nullptr;
}

IsWellformedFunctionPass::AnalysisResult
IsWellformedFunctionPass::getResult() const {
  return IsWellformedPassResult(Sites);
}

} // namespace mir::passes
