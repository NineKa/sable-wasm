#include "bytecode/Module.h"
#include "parser/ByteArrayReader.h"
#include "parser/ExprBuilderDelegate.h"
#include "parser/Parser.h"
#include "parser/customsections/Name.h"

#include <mio/mmap.hpp>

struct Delegate : parser::ExprBuilderDelegate {};

int main() {
  mio::basic_mmap_source<std::byte> File("../test/viu.wasm");
  parser::ByteArrayReader Reader(File.data(), File.size());
  Delegate D;
  parser::Parser Parser(Reader, D);
  Parser.parse();

  parser::customsections::Name N;
  fmt::print("{}\n", N.getFunctionNames().size());
}
