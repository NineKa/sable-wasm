cmake_minimum_required(VERSION 3.8)
project(sable-wasm)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -pedantic -Werror -Wfatal-errors 
  -Wno-unused-parameter
  -Wno-unused-const-variable)

add_compile_options($<$<CONFIG:RELEASE>:-flto>)
add_link_options($<$<CONFIG:RELEASE>:-flto>)

add_compile_options($<$<CONFIG:DEBUG>:-fsanitize=undefined>)
add_link_options($<$<CONFIG:DEBUG>:-fsanitize=undefined>)

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/fmt)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/mio)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/preprocessor)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/utfcpp)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/range-v3)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/cxxopts)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs support core analysis passes target native)

add_library(sablewasm
        src/bytecode/Module.cc
        src/bytecode/Validation.cc
        src/mir/ASTNode.cc
        src/mir/Instruction.cc
        src/mir/Instructions.cc
        src/mir/Branch.cc
        src/mir/Compare.cc
        src/mir/Unary.cc
        src/mir/Binary.cc
        src/mir/Vector.cc
        src/mir/Cast.cc
        src/mir/BasicBlock.cc
        src/mir/InitializerExpr.cc
        src/mir/Module.cc
        src/mir/Function.cc
        src/mir/MIRCodegen.cc
        src/mir/passes/TypeInfer.cc
        src/mir/passes/Dominator.cc
        src/mir/passes/SimplifyCFG.cc
        src/parser/ExprBuilderDelegate.cc
        src/parser/customsections/Name.cc
        src/codegen-llvm-instance/IRBuilder.cc
        src/codegen-llvm-instance/LLVMCodegen.cc
        src/codegen-llvm-instance/EntityLayout.cc
        src/codegen-llvm-instance/TranslationContext.cc
        src/codegen-llvm-instance/TranslationVisitor.cc
        src/codegen-llvm-instance/TranslationCasts.cc)
target_link_libraries(sablewasm
        fmt::fmt
        mio::mio
        Boost::preprocessor
        utf8::cpp
        range-v3
        ${llvm_libs})

add_library(sablewasm-rt SHARED
        src/codegen-llvm-instance/WASI.cc
        src/codegen-llvm-instance/WebAssemblyMemory.cc
        src/codegen-llvm-instance/WebAssemblyGlobal.cc
        src/codegen-llvm-instance/WebAssemblyTable.cc
        src/codegen-llvm-instance/WebAssemblyInstance.cc)
target_link_libraries(sablewasm-rt fmt::fmt-header-only range-v3 dl)

add_executable(sable-wasm src/sable-wasm.cc)
target_link_libraries(sable-wasm sablewasm cxxopts)

add_executable(tester src/Tester.cc)
target_link_libraries(tester sablewasm-rt)

add_executable(validate src/validate.cc)
target_link_libraries(validate sablewasm)
