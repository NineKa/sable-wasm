#ifndef SABLE_INCLUDE_GUARD_BYTECODE_MODULE
#define SABLE_INCLUDE_GUARD_BYTECODE_MODULE

#include "Instruction.h"

#include <variant>

namespace bytecode {
using ImportDescriptor = std::variant<
    bytecode::TypeIDX, bytecode::TableType, bytecode::MemoryType,
    bytecode::GlobalType>;
using ExportDescriptor = std::variant<
    bytecode::FuncIDX, bytecode::TableIDX, bytecode::MemIDX,
    bytecode::GlobalIDX>;

class Module {};
} // namespace bytecode

#endif
