#include "ASTNode.h"

#include <range/v3/algorithm/find_if.hpp>

namespace mir {
ASTNode::ASTNode(ASTNodeKind Kind_) : Kind(Kind_), Name() {}
ASTNode::~ASTNode() noexcept {
  while (!Uses.empty()) Uses.front()->replace(this, nullptr);
}

std::string_view ASTNode::getName() const { return Name; }
void ASTNode::setName(std::string Name_) { Name = std::move(Name_); }
bool ASTNode::hasName() const { return !Name.empty(); }

ASTNodeKind ASTNode::getASTNodeKind() const { return Kind; }

ASTNode::use_site_iterator ASTNode::use_site_begin() const {
  return Uses.begin();
}

ASTNode::use_site_iterator ASTNode::use_site_end() const { return Uses.end(); }

void ASTNode::add_use(ASTNode *Referrer) { Uses.push_front(Referrer); }

namespace {
template <typename Iterator, typename T>
Iterator find_before(Iterator BeforeFist, Iterator Last, T const &Value) {
  assert(BeforeFist != Last);
  auto Next = std::next(BeforeFist);
  if (Next == Last) return Last;
  if (*Next == Value) return BeforeFist;
  return find_before(Next, Last, Value);
}
} // namespace

void ASTNode::remove_use(ASTNode *Referrer) {
  auto SearchIter = find_before(Uses.before_begin(), Uses.end(), Referrer);
  assert(SearchIter != Uses.end());
  assert(*std::next(SearchIter) == Referrer);
  Uses.erase_after(SearchIter);
}

bool ImportableEntity::isImported() const { return Import != nullptr; }

std::string_view ImportableEntity::getImportModuleName() const {
  return std::get<0>(*Import);
}

std::string_view ImportableEntity::getImportEntityName() const {
  return std::get<1>(*Import);
}

void ImportableEntity::setImport(
    std::string ModuleName, std::string EntityName) {
  if (ModuleName.empty() && EntityName.empty()) {
    Import = nullptr;
    return;
  }
  assert(!ModuleName.empty() && !EntityName.empty());
  Import = std::make_unique<ImportDescriptor>(
      std::move(ModuleName), std::move(EntityName));
}

bool ExportableEntity::isExported() const { return Export != nullptr; }

std::string_view ExportableEntity::getExportName() const { return *Export; }

void ExportableEntity::setExport(std::string EntityName) {
  if (EntityName.empty()) {
    Export = nullptr;
    return;
  }
  Export = std::make_unique<ExportDescriptor>(std::move(EntityName));
}

} // namespace mir