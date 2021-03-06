#include "MIRCodegen.h"

namespace mir::bytecode_codegen {
using namespace bytecode::valuetypes;
namespace minsts = mir::instructions;
namespace binsts = bytecode::instructions;
/////////////////////////////// EntityMap //////////////////////////////////////
template <ranges::random_access_range T, typename IndexType>
typename T::value_type
EntityMap::getPtr(T const &Container, IndexType Index) const {
  auto CastedIndex = static_cast<std::size_t>(Index);
  if (!(CastedIndex < Container.size())) return nullptr;
  return Container[CastedIndex];
}

namespace {
namespace detail {
template <typename MEntityType, typename BEntityType>
void setImportExportInfo(MEntityType &MEntity, BEntityType const &BEntity) {
  if (BEntity.isImported()) {
    std::string ModuleName(BEntity.getImportModuleName());
    std::string EntityName(BEntity.getImportEntityName());
    MEntity->setImport(std::move(ModuleName), std::move(EntityName));
  }
  if (BEntity.isExported()) {
    std::string EntityName(BEntity.getExportName());
    MEntity->setExport(std::move(EntityName));
  }
}
} // namespace detail
} // namespace

EntityMap::EntityMap(bytecode::Module const &BModule, Module &MModule) {
  bytecode::ModuleView BModuleView(BModule);
  for (auto const &BMemory : BModuleView.memories()) {
    auto *MMemory = MModule.BuildMemory(*BMemory.getType());
    detail::setImportExportInfo(MMemory, BMemory);
    Memories.push_back(MMemory);
  }
  for (auto const &BTable : BModuleView.tables()) {
    auto *MTable = MModule.BuildTable(*BTable.getType());
    detail::setImportExportInfo(MTable, BTable);
    Tables.push_back(MTable);
  }
  for (auto const &BGlobal : BModuleView.globals()) {
    auto *MGlobal = MModule.BuildGlobal(*BGlobal.getType());
    detail::setImportExportInfo(MGlobal, BGlobal);
    Globals.push_back(MGlobal);
  }
  for (auto const &BFunction : BModuleView.functions()) {
    auto *MFunction = MModule.BuildFunction(*BFunction.getType());
    detail::setImportExportInfo(MFunction, BFunction);
    Functions.push_back(MFunction);
  }
}

Function *EntityMap::operator[](bytecode::FuncIDX Index) const {
  return getPtr(Functions, Index);
}

Memory *EntityMap::operator[](bytecode::MemIDX Index) const {
  return getPtr(Memories, Index);
}

Table *EntityMap::operator[](bytecode::TableIDX Index) const {
  return getPtr(Tables, Index);
}

Global *EntityMap::operator[](bytecode::GlobalIDX Index) const {
  return getPtr(Globals, Index);
}

void EntityMap::annotate(parser::customsections::Name const &Name) {
  for (auto const &FuncNameEntry : Name.getFunctionNames()) {
    auto *MFunction = operator[](FuncNameEntry.FuncIndex);
    MFunction->setName(FuncNameEntry.Name);
  }
  for (auto const &LocalNameEntry : Name.getLocalNames()) {
    auto *MFunction = operator[](LocalNameEntry.FuncIndex);
    auto &MLocal = *std::next(
        MFunction->local_begin(),
        static_cast<std::size_t>(LocalNameEntry.LocalIndex));
    MLocal.setName(LocalNameEntry.Name);
  }
}

Memory *EntityMap::getImplicitMemory() const {
  assert(!Memories.empty());
  return Memories.front();
}

Table *EntityMap::getImplicitTable() const {
  assert(!Tables.empty());
  return Tables.front();
}

////////////////////////// TranslationTask /////////////////////////////////////
class TranslationTask::TranslationContext {
  EntityMap const &E;
  bytecode::views::Function SourceFunction;
  mir::Function &TargetFunction;
  std::vector<mir::Local *> Locals;

  // < Branch Target with Merge Phi Nodes, Num of Phi Nodes>
  std::vector<std::tuple<BasicBlock *, unsigned>> Labels;

public:
  TranslationContext(
      EntityMap const &EntitiesResolver_,
      bytecode::views::Function SourceFunction_, mir::Function &TargetFunction_)
      : E(EntitiesResolver_), SourceFunction(SourceFunction_),
        TargetFunction(TargetFunction_) {
    if constexpr (ranges::sized_range<decltype(SourceFunction.getLocals())>)
      Locals.reserve(ranges::size(SourceFunction.getLocals()));
    for (auto const &LocalType : SourceFunction.getLocals()) {
      auto *Local = TargetFunction.BuildLocal(LocalType);
      Locals.push_back(Local);
    }
  }

  bytecode::views::Function source() const { return SourceFunction; }
  mir::Function &target() { return TargetFunction; }

  Function *operator[](bytecode::FuncIDX Index) const { return E[Index]; }
  Memory *operator[](bytecode::MemIDX Index) const { return E[Index]; }
  Table *operator[](bytecode::TableIDX Index) const { return E[Index]; }
  Global *operator[](bytecode::GlobalIDX Index) const { return E[Index]; }

  Local *operator[](bytecode::LocalIDX Index) const {
    auto CastedIndex = static_cast<std::size_t>(Index);
    assert(CastedIndex < Locals.size());
    return Locals[CastedIndex];
  }

  std::tuple<BasicBlock *, unsigned>
  operator[](bytecode::LabelIDX Index) const {
    auto CastedIndex = static_cast<std::size_t>(Index);
    assert(CastedIndex < Labels.size());
    return Labels[Labels.size() - CastedIndex - 1];
  }

  void pushLabel(BasicBlock *MergeBasicBlock, unsigned NumPhiNodes) {
    Labels.emplace_back(MergeBasicBlock, NumPhiNodes);
  }
  void popLabel() { Labels.pop_back(); }
};

class TranslationTask::TranslationVisitor :
    public bytecode::InstVisitorBase<TranslationVisitor, void> {
  BasicBlock *InsertBefore = nullptr;
  TranslationContext &Context;

  BasicBlock *createBasicBlock() {
    return Context.target().BuildBasicBlock(InsertBefore);
  }

public:
  TranslationVisitor(
      TranslationContext &Context_, BasicBlock *InsertBefore_ = nullptr)
      : InsertBefore(InsertBefore_), Context(Context_) {}
};

} // namespace mir::bytecode_codegen
