#include "parser/ByteArrayReader.h"
#include "parser/Parser.h"

#include <mio/mmap.hpp>

struct Delegate : parser::DelegateBase {
  void onTypeSectionEntry(parser::SizeType, bytecode::FunctionType F) {
    fmt::print("{}\n", F);
  }
};

int main() {
  mio::basic_mmap_source<std::byte> File("../test/viu.wasm");
  parser::ByteArrayReader Reader(File.data(), File.size());
  Delegate D;
  parser::Parser Parser(Reader, D);
  Parser.parse();

  using namespace utility::literals;

  std::array Bytes{0x06_byte, 0xe4_byte, 0xbd_byte, 0xa0_byte,
                   0xe5_byte, 0xa5_byte, 0xbd_byte};
  parser::ByteArrayReader R2(Bytes.data(), Bytes.size());
  parser::WASMReader R(R2);
  auto StrView = R.readUTF8StringVector();
  fmt::print("{}\n", StrView);
}
