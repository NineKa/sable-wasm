#include "codegen-llvm-instance/WASI.h"
#include "codegen-llvm-instance/WebAssemblyInstance.h"

int main(int argc, char const *argv[]) {
  using namespace runtime;
  utility::ignore(argc, argv);

#define WASI_IMPORT(name, func)                                                \
  InstanceBuilder.tryImport("wasi_snapshot_preview1", name, func)

  auto InstanceBuilder = WebAssemblyInstanceBuilder(argv[1]);
  WASI_IMPORT("proc_exit", wasi::proc_exit);
  WASI_IMPORT("clock_time_get", wasi::clock_time_get);
  WASI_IMPORT("args_sizes_get", wasi::args_sizes_get);
  WASI_IMPORT("args_get", wasi::args_get);
  WASI_IMPORT("fd_prestat_get", wasi::fd_prestart_get);
  WASI_IMPORT("fd_prestat_dir_name", wasi::fd_prestat_dir_name);
  WASI_IMPORT("path_open", wasi::path_open);
  WASI_IMPORT("fd_read", wasi::fd_read);
  WASI_IMPORT("fd_seek", wasi::fd_seek);
  WASI_IMPORT("fd_close", wasi::fd_close);
  WASI_IMPORT("fd_fdstat_get", wasi::fd_fdstat_get);
  WASI_IMPORT("fd_fdstat_set_flags", wasi::fd_fdstat_set_flags);
  WASI_IMPORT("fd_write", wasi::fd_write);
  WASI_IMPORT("random_get", wasi::random_get);
  WASI_IMPORT("poll_oneoff", wasi::poll_oneoff);
  auto Instance = InstanceBuilder.Build();

  try {
    Instance->getFunction("_start").invoke<void>();
  } catch (runtime::wasi::exceptions::WASIExit const &Exception) {
    return Exception.getExitCode();
  }
  return 0;
}