cmake_minimum_required(VERSION 3.25)

enable_testing()

project(CiallangTests CXX)

find_package(Catch2 3 REQUIRED)

include(CTest)
include(Catch)

set(TEST_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(TEST_FILES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/test_files)
configure_file(test_config.h.in test_config.h)

set(SOURCES
        gc_test.cpp
        lexer_test.cpp
        interpreter_test.cpp
        table_formatter_test.cpp
        expression_test.cpp
        function_test.cpp
        statement_test.cpp
        parser_comprehensive_test.cpp
        lexer_comprehensive_test.cpp
        gc_comprehensive_test.cpp
        type_system_test.cpp
        vm_instruction_test.cpp
        integration_test.cpp
)

string(REPLACE ".cpp" "" BASENAMES_SOURCES "${SOURCES}")
set(TARGETS ${BASENAMES_SOURCES})

foreach(name ${TARGETS})
    add_executable(${name} ${name}.cpp main.cpp)
endforeach()

set(ALL_TARGETS ${TARGETS})

foreach(name ${ALL_TARGETS})
    target_link_libraries(${name}
        PRIVATE
            Catch2::Catch2
            Ciallang
    )
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    catch_discover_tests(${name})
endforeach()