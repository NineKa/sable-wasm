#include "codegen-llvm-instance/WASI.h"
#include "codegen-llvm-instance/WebAssemblyInstance.h"

int main(int argc, char const *argv[]) {
  using namespace runtime;
  utility::ignore(argc, argv);

  auto Instance =
      WebAssemblyInstanceBuilder("./out.so")
          .import("wasi_snapshot_preview1", "proc_exit", wasi::proc_exit)
          .Build();

  try {
    Instance->getFunction("_start")->invoke<void>();
  } catch (std::exception const &Exception) {
    fmt::print("{}\n", Exception.what());
  }

  return 0;
}