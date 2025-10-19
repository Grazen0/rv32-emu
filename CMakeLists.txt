cmake_minimum_required(VERSION 3.11)
project(rv32_emu LANGUAGES C VERSION 0.1.0)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

set(GCC_LIKE $<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>)

set(sources
    src/cpu.c
    src/elf_util.c
    src/io.c
    src/log.c
    src/numeric.c
    src/protocol.c
    src/macros.c
    src/memory.c
    src/stdinc.c
    src/str.c)

add_library(argparse STATIC external/argparse/argparse.c)
target_include_directories(argparse SYSTEM PUBLIC external/argparse)

add_library(rv32_emu_lib ${sources})
set_target_properties(rv32_emu_lib PROPERTIES C_CLANG_TIDY "clang-tidy")
target_include_directories(rv32_emu_lib PUBLIC src)
target_compile_options(rv32_emu_lib PUBLIC
    $<$<BOOL:${GCC_LIKE}>:-Wall>
    $<$<BOOL:${GCC_LIKE}>:-Wextra>
    $<$<BOOL:${GCC_LIKE}>:-Wpedantic>
    $<$<BOOL:${GCC_LIKE}>:-Wconversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

add_executable(rv32_emu src/main.c)
set_target_properties(rv32_emu PROPERTIES C_CLANG_TIDY "clang-tidy")
set_target_properties(rv32_emu PROPERTIES OUTPUT_NAME "rv32-emu")
target_link_libraries(rv32_emu PRIVATE rv32_emu_lib)
target_link_libraries(rv32_emu PRIVATE argparse)

install(TARGETS rv32_emu RUNTIME DESTINATION bin)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
