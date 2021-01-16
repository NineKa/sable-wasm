#include "bytecode/Module.h"
#include "parser/ByteArrayReader.h"
#include "parser/ExprBuilderDelegate.h"
#include "parser/Parser.h"
#include "parser/customsections/Name.h"

#include <mio/mmap.hpp>

struct Delegate : parser::ExprBuilderDelegate {};

int main() {
  mio::basic_mmap_source<std::byte> File(
      "../test/polybench-c-4.2.1-beta/2mm.wasm");
  parser::ByteArrayReader Reader(File.data(), File.size());
  Delegate D;
  parser::customsections::Name N;

  parser::Parser Parser(Reader, D);
  Parser.registerCustomSection(N);
  Parser.parse();

  for (auto const &[FuncIDX, Name] : N.getFunctionNames()) {
    fmt::print("{:2} {}\n", FuncIDX, Name);
  }
  fmt::print(
      "{}\n", N.getFunctionName(static_cast<bytecode::FuncIDX>(0)).value());
}
