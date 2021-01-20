cmake_minimum_required(VERSION 3.8)
project(sable-wasm)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-fno-rtti)
add_compile_options(-Wall -Werror -Wextra -pedantic -Wfatal-errors)

add_compile_options($<$<CONFIG:RELEASE>:-flto>)
add_link_options($<$<CONFIG:RELEASE>:-flto>)

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/fmt)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/mio)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/preprocessor)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/utfcpp)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/range-v3)

add_library(sablewasm
        src/bytecode/Module.cc
        src/bytecode/Validation.cc
        src/parser/ExprBuilderDelegate.cc
        src/parser/customsections/Name.cc)

target_link_libraries(sablewasm
        fmt::fmt
        mio::mio
        Boost::preprocessor
        utf8::cpp
        range-v3)

add_executable(sable-wasm src/Main.cc)
target_link_libraries(sable-wasm sablewasm)
