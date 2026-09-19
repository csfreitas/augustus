cmake_minimum_required(VERSION 3.16)
project(xml_parser_depth_tests C)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(XML_PARSER_SOURCE "${ROOT}/src/core/xml_parser.c" CACHE FILEPATH "Parser implementation under test")
add_executable(xml_parser_depth_tests
    xml_parser_depth_test.c
    ${XML_PARSER_SOURCE}
    ${ROOT}/ext/sxml/sxml.c)
target_include_directories(xml_parser_depth_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${ROOT}/src ${ROOT}/ext)
if(MSVC)
    target_compile_options(xml_parser_depth_tests PRIVATE /W3 /utf-8)
    target_compile_definitions(xml_parser_depth_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
enable_testing()
add_test(NAME xml_parser_single_element_text COMMAND xml_parser_depth_tests root-text)
add_test(NAME xml_parser_single_element_empty COMMAND xml_parser_depth_tests root-empty)
add_test(NAME xml_parser_nested_depth_and_lifecycle COMMAND xml_parser_depth_tests nested)
