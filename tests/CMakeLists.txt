find_package(Catch2 3 REQUIRED)

set(TEST_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(TEST_FILES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/test_files)
set(TEST_FUZZ_REGRESS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/fuzz_regress)

configure_file(test_config.h.in test_config.h)

# 新增 C++ 测试文件须显式登记；未列入此目标的用例不会被 CTest 发现。
set(SOURCES
        bytecode_test.cpp
        runtime_lifetime_test.cpp
        fuzz_regress_test.cpp
        compat_reserved_test.cpp
        native_binding_test.cpp
        native_registration_test.cpp
        native_function_test.cpp
        value_storage_test.cpp
        preprocessor_test.cpp
        ieee_test.cpp
        integration_test.cpp
        interpreter_test.cpp
        optimizer_test.cpp
        ssa_integration_test.cpp
        ssa_test.cpp
        execution_trace_test.cpp
        tail_recursion_test.cpp
        jit_test.cpp
        tjsbc_reader_test.cpp
        tjsbc_translator_test.cpp
        scripts_test.cpp
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

# 每份脚本独立进程，防止跨用例全局注册、RegExp.last、GC 等状态相互污染。
# 语言断言写在 TJS 中；原生绑定、AST、GC 和 VM API 仍由上面的 Catch2 覆盖。
set(CIAL_TJS_TEST_REFERENCE "" CACHE FILEPATH "Optional TJSCompiler CLI for the TJS script suite")
file(GLOB_RECURSE TJS_TEST_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tjs/*.tjs")
list(FILTER TJS_TEST_FILES EXCLUDE REGEX "/assert\\.tjs$")
function(register_tjs_tests prefix engine)
    foreach(script IN LISTS TJS_TEST_FILES)
        file(RELATIVE_PATH case_name "${CMAKE_CURRENT_SOURCE_DIR}/tjs" "${script}")
        string(REPLACE ".tjs" "" case_name "${case_name}")
        string(REPLACE "/" "." case_name "${case_name}")
        add_test(NAME "${prefix}.${case_name}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_tjs_tests.py"
                    --engine "${engine}" "${script}")
        set_tests_properties("${prefix}.${case_name}" PROPERTIES TIMEOUT 20 LABELS "tjs;${prefix}")
    endforeach()
endfunction()
if(Python3_Interpreter_FOUND)
    if(TARGET cial)
        register_tjs_tests(CiallangTjs "$<TARGET_FILE:cial>")
    endif()
    if(CIAL_TJS_TEST_REFERENCE)
        register_tjs_tests(ReferenceTjs "${CIAL_TJS_TEST_REFERENCE}")
    endif()
endif()
if(Python3_Interpreter_FOUND AND TARGET cial)
    add_test(NAME CiallangTjsRunner
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_tjs_runner.py"
                --engine "$<TARGET_FILE:cial>")
    set_tests_properties(CiallangTjsRunner PROPERTIES TIMEOUT 30)
    add_test(NAME CiallangCli
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cli_test.py"
                --cial "$<TARGET_FILE:cial>" --expected-version "${CIAL_VERSION_STRING}")
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

if(Python3_Interpreter_FOUND)
    add_test(NAME CiallangPackaging
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_packaging.py")
    set_tests_properties(CiallangPackaging PROPERTIES TIMEOUT 30)
endif()
