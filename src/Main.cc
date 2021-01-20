#include "bytecode/Module.h"
#include "parser/ByteArrayReader.h"
#include "parser/ModuleBuilderDelegate.h"
#include "parser/Parser.h"

#include <mio/mmap.hpp>

int main(int argc, char const *argv[]) {
  utility::ignore(argc, argv);
  mio::basic_mmap_source<std::byte> Source("../test/viu.wasm");
  parser::ByteArrayReader Reader(Source);
  parser::ModuleBuilderDelegate D;
  parser::Parser Parser(Reader, D);
  Parser.parse();

  auto &Module = D.getModule();
  fmt::print("{}\n", Module.Imports.size());
  fmt::print("{}\n", sizeof(bytecode::views::Function));

  bytecode::ModuleView View(D.getModule());
  for (auto Function : View.functions()) {
    if (!Function.isExported()) continue;
    fmt::print("{}\n{}\n", Function.getExportName(), *Function.getType());
  }
}
