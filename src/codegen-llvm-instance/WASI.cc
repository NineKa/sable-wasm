#include "WASI.h"

#include "WebAssemblyInstance.h"

namespace runtime::wasi {
void proc_exit(__sable_instance_t *, std::int32_t ExitCode) {
  throw exceptions::WASIExit(ExitCode);
}
} // namespace runtime::wasi