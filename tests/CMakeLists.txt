find_package(Catch2 3 REQUIRED)

set(TEST_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(TEST_FILES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/test_files)
set(TEST_FUZZ_REGRESS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/fuzz_regress)

configure_file(test_config.h.in test_config.h)

set(SOURCES
        bytecode_test.cpp
        runtime_lifetime_test.cpp
        fuzz_regress_test.cpp
        compat_reserved_test.cpp
        expression_test.cpp
        function_test.cpp
        native_binding_test.cpp
        native_registration_test.cpp
        native_function_test.cpp
        value_storage_test.cpp
        preprocessor_test.cpp
        class_extends_test.cpp
        ieee_test.cpp
        integration_test.cpp
        interpreter_test.cpp
        loop_test.cpp
        optimizer_test.cpp
        ssa_integration_test.cpp
        ssa_test.cpp
        execution_trace_test.cpp
        tail_recursion_test.cpp
        jit_test.cpp
        judgment_test.cpp
        oop_test.cpp
        property_test.cpp
        tjsbc_reader_test.cpp
        tjsbc_translator_test.cpp
        scripts_test.cpp
        scope_test.cpp
        statement_test.cpp
        try_catch_test.cpp
        type_system_test.cpp
)

add_executable(CiallangTests ${SOURCES})
if(MSVC)
    target_compile_options(CiallangTests PRIVATE /EHsc /MP /utf-8)
endif()

target_link_libraries(CiallangTests
    PRIVATE
        Catch2::Catch2WithMain
        Ciallang
)

target_include_directories(CiallangTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

include(Catch)

# Windows 下 CTest/CMake 按 ANSI 代码页转码测试名与过滤器（CI runner 为 1252），
# 中文用例名经 discovery/传参链路后变成 "？?" 导致 No test cases matched；
# 本机开启系统级 UTF-8 所以无感。Windows 改为单条测试跑整个套件绕开按名过滤。
if(WIN32)
    add_test(NAME CiallangSuite COMMAND CiallangTests)
else()
    catch_discover_tests(CiallangTests)
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND TARGET cial)
    add_test(NAME CiallangCli
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cli_test.py"
                --cial "$<TARGET_FILE:cial>")
    set_tests_properties(CiallangCli PROPERTIES TIMEOUT 60)
    add_test(NAME CiallangLspProtocol
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/lsp/test_protocol.py"
                --cial "$<TARGET_FILE:cial>")
    set_tests_properties(CiallangLspProtocol PROPERTIES TIMEOUT 30)
    add_test(NAME CiallangBytecodeTool
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../bench/test_bytecode_conformance.py"
                --cial "$<TARGET_FILE:cial>")
    set_tests_properties(CiallangBytecodeTool PROPERTIES TIMEOUT 60)
endif()
