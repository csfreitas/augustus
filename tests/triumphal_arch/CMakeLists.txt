cmake_minimum_required(VERSION 3.16)
project(triumphal_arch_regression C)
get_filename_component(DEFAULT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(SOURCE_ROOT "${DEFAULT_ROOT}" CACHE PATH "Augustus source tree under test")
option(ARCH_SAVE_PROBE "Build the optional read-only probe for local save copies (PowerShell required)" OFF)
add_executable(arch_test arch_test.c
    ${SOURCE_ROOT}/src/building/menu.c
    ${SOURCE_ROOT}/src/scenario/allowed_building.c
    ${SOURCE_ROOT}/src/city/buildings.c
    ${SOURCE_ROOT}/src/core/buffer.c
    ${SOURCE_ROOT}/src/game/resource.c)
target_include_directories(arch_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated ${SOURCE_ROOT}/src ${SOURCE_ROOT}/ext)
target_compile_definitions(arch_test PRIVATE _CRT_SECURE_NO_WARNINGS)
if(ARCH_SAVE_PROBE)
    find_program(POWERSHELL NAMES pwsh powershell REQUIRED)
    execute_process(COMMAND ${POWERSHELL} -NoProfile -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/extract-decoders.ps1 -SourceRoot ${SOURCE_ROOT} -Destination ${CMAKE_CURRENT_BINARY_DIR}/generated RESULT_VARIABLE extraction)
    if(NOT extraction EQUAL 0)
        message(FATAL_ERROR "Decoder extraction failed")
    endif()
    target_sources(arch_test PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated/scenario_sizes.c
        ${CMAKE_CURRENT_BINARY_DIR}/generated/city_decoder.c
        ${SOURCE_ROOT}/src/core/memory_block.c
        ${SOURCE_ROOT}/src/core/zip.c
        ${SOURCE_ROOT}/src/core/zlib_helper.c)
    target_compile_definitions(arch_test PRIVATE ARCH_SAVE_PROBE MINIZ_IMPLEMENTATION)
endif()
if(MSVC)
    target_compile_options(arch_test PRIVATE /W3 /utf-8)
endif()
enable_testing()
add_test(NAME arch_reward_lifecycle COMMAND arch_test)
