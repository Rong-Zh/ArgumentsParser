cmake_minimum_required(VERSION 3.14)

project(ArgumentsParser LANGUAGES CXX)

option(ARGUMENTS_PARSER_BUILD_TESTS "Build ArgumentsParser tests" OFF)
option(ARGUMENTS_PARSER_BUILD_EXAMPLES "Build ArgumentsParser examples" OFF)
option(ARGUMENTS_PARSER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(ArgumentsParser INTERFACE)
add_library(ArgumentsParser::ArgumentsParser ALIAS ArgumentsParser)

target_compile_features(ArgumentsParser INTERFACE cxx_std_11)
target_include_directories(ArgumentsParser INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

function(arguments_parser_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(ARGUMENTS_PARSER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(ARGUMENTS_PARSER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

if(ARGUMENTS_PARSER_BUILD_EXAMPLES)
    add_executable(arguments_parser_example tests/example1.cpp)
    target_link_libraries(arguments_parser_example PRIVATE ArgumentsParser::ArgumentsParser)
    arguments_parser_enable_warnings(arguments_parser_example)

    add_executable(subcommand_parser_example tests/example2.cpp)
    target_link_libraries(subcommand_parser_example PRIVATE ArgumentsParser::ArgumentsParser)
    arguments_parser_enable_warnings(subcommand_parser_example)
endif()

if(ARGUMENTS_PARSER_BUILD_TESTS)
    include(CTest)
    find_package(GTest REQUIRED)

    function(arguments_parser_add_test name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE ArgumentsParser::ArgumentsParser GTest::gtest_main)
        arguments_parser_enable_warnings(${name})
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    arguments_parser_add_test(test_argument_parser tests/test_argument_parser.cpp)
    arguments_parser_add_test(test_subcommand_parser tests/test_subcommand_parser.cpp)
    arguments_parser_add_test(test_output_stream tests/test_output_stream.cpp)

    add_executable(test_include_both tests/test_include_both.cpp)
    target_link_libraries(test_include_both PRIVATE ArgumentsParser::ArgumentsParser)
    arguments_parser_enable_warnings(test_include_both)
    add_test(NAME test_include_both COMMAND test_include_both)

    add_executable(test_include_both_reverse tests/test_include_both_reverse.cpp)
    target_link_libraries(test_include_both_reverse PRIVATE ArgumentsParser::ArgumentsParser)
    arguments_parser_enable_warnings(test_include_both_reverse)
    add_test(NAME test_include_both_reverse COMMAND test_include_both_reverse)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
    TARGETS ArgumentsParser
    EXPORT ArgumentsParserTargets
)
install(
    FILES ArgumentParser.h SubCommandParser.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    EXPORT ArgumentsParserTargets
    FILE ArgumentsParserTargets.cmake
    NAMESPACE ArgumentsParser::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ArgumentsParser
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArgumentsParserConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ArgumentsParserConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ArgumentsParser
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/ArgumentsParserConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ArgumentsParser
)
