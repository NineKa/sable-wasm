cmake_minimum_required(VERSION 3.8)
project(sable-wasm)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
#add_compile_options(-fno-rtti)
add_compile_options(-Wall -Werror -Wextra -pedantic -Wfatal-errors)
add_compile_options(-Wno-unknown-pragmas)

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/fmt          EXCLUDE_FROM_ALL)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/mio          EXCLUDE_FROM_ALL)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/poly_vector  EXCLUDE_FROM_ALL) 
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/preprocessor EXCLUDE_FROM_ALL)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/utfcpp       EXCLUDE_FROM_ALL)
include_directories(${PROJECT_SOURCE_DIR}/lib/cppitertools)

add_executable(sable-wasm src/Main.cc)
target_link_libraries(sable-wasm
    fmt::fmt 
    mio::mio 
    PolyVector 
    Boost::preprocessor 
    utf8::cpp)