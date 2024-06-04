cmake_minimum_required(VERSION 3.25)

enable_testing()

project(Ciallang CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

macro(get_source_file directory)
    file(GLOB ${directory}_SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/${directory}/*.[ch]pp")
endmacro()

get_source_file(ast)
get_source_file(common)
get_source_file(gen)
get_source_file(lexer)
get_source_file(parser)
get_source_file(types)
get_source_file(vm)

add_executable(
        vm_instruction_test
        vm/instruction_test.cpp

        ${CMAKE_SOURCE_DIR}/src/init/GlogInit.hpp

        ${ast_SOURCE_FILES}
        ${common_SOURCE_FILES}
        ${gen_SOURCE_FILES}
        ${lexer_SOURCE_FILES}
        ${parser_SOURCE_FILES}
        ${types_SOURCE_FILES}
        ${vm_SOURCE_FILES}
)

target_include_directories(vm_instruction_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

include(cmake/depend.cmake)
find_package(gtest REQUIRED)

include(../cmake/depend.cmake)
find_package(fmt REQUIRED)
find_package(utf8proc REQUIRED)
find_package(frozen REQUIRED)
find_package(gflags REQUIRED)
find_package(glog REQUIRED)

target_link_libraries(
        vm_instruction_test
        PRIVATE
        GTest::gtest_main
        fmt utf8proc frozen  gflags glog gtest
)

target_precompile_headers(vm_instruction_test PRIVATE "${CMAKE_SOURCE_DIR}/src/pch.h")


include(GoogleTest)
gtest_discover_tests(vm_instruction_test)
