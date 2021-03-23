#include "ASTNode.h"

namespace mir {
ASTNode::ASTNode(ASTNodeKind Kind_) : Kind(Kind_), Name() {}
ASTNode::~ASTNode() noexcept {
  for (auto *U : Uses) U->detach(this);
}

std::string_view ASTNode::getName() const { return Name; }
void ASTNode::setName(std::string Name_) { Name = std::move(Name_); }
bool ASTNode::hasName() const { return !Name.empty(); }

ASTNodeKind ASTNode::getASTNodeKind() const { return Kind; }

using Iterator = ASTNode::use_site_iterator;
Iterator ASTNode::use_site_begin() const { return Uses.begin(); }
Iterator ASTNode::use_site_end() const { return Uses.end(); }

void ASTNode::add_use(ASTNode *Referrer) { Uses.push_front(Referrer); }
void ASTNode::remove_use(ASTNode *Referrer) { std::erase(Uses, Referrer); }

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