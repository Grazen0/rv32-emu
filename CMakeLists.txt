cmake_minimum_required(VERSION 3.11)
project(rv32_emu LANGUAGES C VERSION 0.1.0)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(sources
    src/cpu.c
    src/gdb.c
    src/macros.c
    src/stdinc.c
    src/str.c
    src/util.c)

add_library(argparse STATIC external/argparse/argparse.c)
target_include_directories(argparse PUBLIC external/argparse)

add_library(rv32_emu_lib ${sources})
target_include_directories(rv32_emu_lib PUBLIC src)

add_executable(rv32_emu src/main.c)
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
