cmake_minimum_required(VERSION 3.25)

enable_testing()

project(CiallangTests CXX)

find_package(Catch2 3 REQUIRED)

set(TEST_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(TEST_FILES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/test_files)
configure_file(test_config.h.in test_config.h)

set(SOURCES
        expression_test.cpp
        function_test.cpp
        native_function_test.cpp
        preprocessor_test.cpp
        class_extends_test.cpp
        #        ieee_test.cpp
        interpreter_test.cpp
        #        integration_test.cpp
        loop_test.cpp
        judgment_test.cpp
        oop_test.cpp
        property_test.cpp
        scope_test.cpp
#        statement_test.cpp
#        type_system_test.cpp
)

add_executable(CiallangTests ${SOURCES})

target_link_libraries(CiallangTests
    PRIVATE
        Catch2::Catch2WithMain
        Ciallang
)

target_include_directories(CiallangTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

include(CTest)
include(Catch)

catch_discover_tests(CiallangTests)
