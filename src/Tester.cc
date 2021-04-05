#include "codegen-llvm-instance/WASI.h"
#include "codegen-llvm-instance/WebAssemblyInstance.h"

int main(int argc, char const *argv[]) {
  using namespace runtime;
  utility::ignore(argc, argv);

  try {
    auto Instance = WebAssemblyInstanceBuilder("./out.sable").Build();
    auto &Table = Instance->getTable("table");
    auto Callee = Table.get(0);
    auto Result = Callee.invoke<std::int32_t>(1, 1);
    fmt::print("{}\n", Result);
  } catch (std::exception const &Exception) {
    fmt::print("{}\n", Exception.what());
  }

  return 0;
}