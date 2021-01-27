#include "mir/Instruction.h"
#include <fmt/format.h>

#include <mio/mmap.hpp>

int main(int argc, char const *argv[]) {
  (void)argc;
  (void)argv;

  using namespace mir;
  using namespace mir::instructions;
  fmt::print(
      "{}\n{}\n{}\n", sizeof(InstResultType), sizeof(instructions::Unreachable),
      sizeof(bytecode::ValueType));
}
