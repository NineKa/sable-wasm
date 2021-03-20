#include "IsWellformedPass.h"

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

namespace {
namespace detail {
template <typename T>
class AddrEmplaceIterator :
    public std::iterator<std::output_iterator_tag, typename T::value_type> {
  T *C;

public:
  using container_type = T;
  AddrEmplaceIterator() = default;
  explicit AddrEmplaceIterator(T &C_) : C(std::addressof(C_)) {}
  AddrEmplaceIterator(AddrEmplaceIterator const &) = default;
  AddrEmplaceIterator(AddrEmplaceIterator &&) noexcept = default;
  AddrEmplaceIterator &operator=(AddrEmplaceIterator const &) = default;
  AddrEmplaceIterator &operator=(AddrEmplaceIterator &&) noexcept = default;
  ~AddrEmplaceIterator() noexcept = default;

  AddrEmplaceIterator &operator*() { return *this; }
  AddrEmplaceIterator &operator++() { return *this; }
  AddrEmplaceIterator &operator++(int) { return *this; }
  template <typename U> AddrEmplaceIterator &operator=(U &&Arg) {
    C->emplace(std::addressof(Arg));
    return *this;
  }
};
} // namespace detail
} // namespace

void IsWellformedModulePass::run(mir::Module &Module) {
  detail::AddrEmplaceIterator Iterator(AvailableNodes);
  ranges::copy(Module.getMemories(), Iterator);
}

} // namespace mir::passes
