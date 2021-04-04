#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_WASI
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_WASI

#include <cstdint>

#include <stdexcept>

#include <fmt/format.h>

extern "C" {
struct __sable_instance_t;
}

namespace runtime::wasi {
namespace exceptions {
class WASIExit : public std::runtime_error {
  std::int32_t ExitCode;

public:
  WASIExit(std::int32_t ExitCode_)
      : std::runtime_error(fmt::format("wasi exit with {}", ExitCode_)),
        ExitCode(ExitCode_) {}
  std::int32_t getExitCode() const { return ExitCode; }
};
} // namespace exceptions

void proc_exit(__sable_instance_t *, std::int32_t ExitCode);
} // namespace runtime::wasi

#endif