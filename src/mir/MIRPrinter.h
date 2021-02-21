#ifndef SABLE_INCLUDE_GUARD_MIR_PRINTER
#define SABLE_INCLUDE_GUARD_MIR_PRINTER

#include "Instruction.h"
#include "Module.h"

#include <fmt/format.h>

#include <iterator>
#include <unordered_map>

namespace mir::printer {

class EntityNameResolver {
  std::unordered_map<ASTNode const *, std::size_t> Names;
  template <ranges::input_range T> void prepareEntities(T const &Entities) {
    std::size_t UnnamedCounter = 0;
    for (auto const &Entity : Entities) {
      if (Entity.hasName()) continue;
      Names.emplace(std::addressof(Entity), UnnamedCounter);
      UnnamedCounter = UnnamedCounter + 1;
    }
  }

  template <std::output_iterator<char> Iterator>
  Iterator operator()(Iterator Out, char const *FmtStr, ASTNode const &Node) {
    auto const *NodePtr = std::addressof(Node);
    if (Node.hasName()) return fmt::format_to(Out, "%{}", Node.getName());
    if (!Names.contains(NodePtr))
      return fmt::format_to(Out, FmtStr, fmt::ptr(NodePtr));
    auto UnnamedIndex = std::get<1>(*Names.find(NodePtr));
    return fmt::format_to(Out, FmtStr, UnnamedIndex);
  }

public:
  EntityNameResolver() = default;
  EntityNameResolver(Module const &Module_) {
    prepareEntities(Module_.getMemories());
    prepareEntities(Module_.getTables());
    prepareEntities(Module_.getGlobals());
    prepareEntities(Module_.getFunctions());
  }

  template <std::output_iterator<char> Iterator>
  Iterator operator()(Iterator Out, Memory const &Memory_) {
    return operator()(Out, "%memory:{}", Memory_);
  }

  template <std::output_iterator<char> Iterator>
  Iterator operator()(Iterator Out, Table const &Table_) {
    return operator()(Out, "%table:{}", Table_);
  }

  template <std::output_iterator<char> Iterator>
  Iterator operator()(Iterator Out, Global const &Global_) {
    return operator()(Out, "%global:{}", Global_);
  }

  template <std::output_iterator<char> Iterator>
  Iterator operator()(Iterator Out, Function const &Function_) {
    return operator()(Out, "%function:{}", Function_);
  }
};

class LocalNameResolver {
  std::unordered_map<ASTNode const *, std::size_t> Names;

  template <std::output_iterator<char> Iterator>
  Iterator operator()(Iterator Out, char const *FmtStr, ASTNode const &Node) {
    auto const *NodePtr = std::addressof(Node);
    if (Node.hasName()) return fmt::format_to(Out, "%{}", Node.getName());
    if (!Names.contains(NodePtr))
      return fmt::format_to(Out, FmtStr, fmt::ptr(NodePtr));
    auto UnnamedIndex = std::get<1>(*Names.find(NodePtr));
    return fmt::format_to(Out, FmtStr, UnnamedIndex);
  }

public:
  LocalNameResolver() = default;
  LocalNameResolver(Function const &Function_) {
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
  Iterator operator()(Iterator Out, Local const &Local_) {
    return operator()(Out, "%{}", Local_);
  }

  template <std::output_iterator<char> Iterator>
  Iterator operator()(Iterator Out, BasicBlock const &BasicBlock_) {
    return operator()(Out, "%BB:{}", BasicBlock_);
  }

  template <std::output_iterator<char> Iterator>
  Iterator operator()(Iterator Out, Instruction const &Instruction_) {
    return operator()(Out, "%{}", Instruction_);
  }
};

struct PrintPolicyBase {};

} // namespace mir::printer

#endif
