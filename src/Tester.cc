#include "codegen-llvm-instance/WASI.h"
#include "codegen-llvm-instance/WebAssemblyInstance.h"

int main(int argc, char const *argv[]) {
  using namespace runtime;
  utility::ignore(argc, argv);

  auto Instance = WebAssemblyInstanceBuilder("./out.so").Build();

  try {
    fmt::print(
        "__heap_base = {}\n", Instance->getGlobal("__heap_base")->asI32());
    fmt::print(
        "__data_end  = {}\n", Instance->getGlobal("__data_end")->asI32());
    auto Result = Instance->getFunction("fibonacci")->invoke<std::int32_t>(10);
    fmt::print("{}\n", Result);
  } catch (std::exception const &Exception) {
    fmt::print("{}\n", Exception.what());
  }

  return 0;
}