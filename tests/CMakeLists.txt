enable_testing()

add_library(unity STATIC ${PROJECT_SOURCE_DIR}/external/unity/unity.c)
target_include_directories(unity PUBLIC ${PROJECT_SOURCE_DIR}/external/unity)

set(test_sources test_str.c)

# Generate test runners for each test file
foreach(test_source ${test_sources})
  set(test_source_path "${CMAKE_CURRENT_SOURCE_DIR}/${test_source}")

  get_filename_component(test_name ${test_source} NAME_WE)
  set(runner_source "${CMAKE_CURRENT_BINARY_DIR}/${test_name}_runner.c")
  set(test_exec "rv32_emu_${test_name}")

  add_custom_command(
    OUTPUT ${runner_source}
    COMMAND
      ${CMAKE_COMMAND} -E env ruby
      ${PROJECT_SOURCE_DIR}/tools/generate_test_runner.rb ${test_source_path}
      ${runner_source}
    DEPENDS ${test_source_path}
    COMMENT "Generating Unity test runner for ${test_source}")

  add_executable(${test_exec} ${test_source_path} ${runner_source})
  target_link_libraries(${test_exec} PRIVATE rv32_emu_lib)
  target_link_libraries(${test_exec} PRIVATE unity)

  add_test(NAME ${test_name} COMMAND ${test_exec})
endforeach()
