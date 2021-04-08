#ifndef SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_WASI
#define SABLE_INCLUDE_GUARD_CODEGEN_LLVM_CTX_WASI

#include "WASITypes.h"

#include <fmt/format.h>

#include <cstdint>
#include <stdexcept>

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

// clang-format off
void proc_exit(__sable_instance_t *, std::int32_t);

inline std::int32_t fd_prestart_get(__sable_instance_t *, std::int32_t, std::int32_t) { return ERRNO_BADF; }
inline std::int32_t fd_prestat_dir_name(__sable_instance_t *, std::int32_t, std::int32_t, std::int32_t) { return ERRNO_BADF; }
inline std::int32_t path_open(__sable_instance_t *, std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int64_t, std::int64_t, std::int32_t, std::int32_t) { return ERRNO_BADF; }
std::int32_t fd_seek(__sable_instance_t *, std::int32_t, std::int64_t, std::int32_t, std::int32_t);
std::int32_t fd_close(__sable_instance_t*, std::int32_t);
std::int32_t fd_fdstat_get(__sable_instance_t *, std::int32_t, std::int32_t);
inline std::int32_t fd_fdstat_set_flags(__sable_instance_t *, std::int32_t, std::int32_t) { return ERRNO_BADF; }
inline std::int32_t fd_read(__sable_instance_t *, std::int32_t, std::int32_t, std::int32_t, std::int32_t) { return ERRNO_BADF; }
std::int32_t fd_write(__sable_instance_t *, std::int32_t, std::int32_t, std::int32_t, std::int32_t);
std::int32_t args_sizes_get(__sable_instance_t *, std::int32_t, std::int32_t);
std::int32_t args_get(__sable_instance_t *, std::int32_t, std::int32_t);

std::int32_t random_get(__sable_instance_t *, std::int32_t, std::int32_t);
std::int32_t clock_time_get(__sable_instance_t *, std::int32_t, std::int64_t, std::int32_t);

std::int32_t poll_oneoff(__sable_instance_t *, std::int32_t, std::int32_t, std::int32_t, std::int32_t) { return ERRNO_INVAL; }
// clang-format on
} // namespace runtime::wasi

#endif