#include "codegen-llvm-instance/WASI.h"
#include "codegen-llvm-instance/WebAssemblyInstance.h"

int main(int argc, char const *argv[]) {
  using namespace runtime;
  utility::ignore(argc, argv);

#define WASI_IMPORT(name, func) import("wasi_snapshot_preview1", name, func)
  auto Instance = WebAssemblyInstanceBuilder("./2mm.sable")
                      .WASI_IMPORT("proc_exit", wasi::proc_exit)
                      .WASI_IMPORT("fd_seek", wasi::fd_seek)
                      .WASI_IMPORT("fd_close", wasi::fd_close)
                      .WASI_IMPORT("fd_fdstat_get", wasi::fd_fdstat_get)
                      .WASI_IMPORT("fd_write", wasi::fd_write)
                      .WASI_IMPORT("args_get", wasi::args_get)
                      .WASI_IMPORT("args_sizes_get", wasi::args_sizes_get)
                      .Build();
  Instance->getFunction("_start").invoke<void>();
  return 0;
}