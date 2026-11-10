cmake_minimum_required(VERSION 3.25)

# vcpkg reads features during project(), so resolve optional targets first.
set(CIAL_TOP_LEVEL OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CIAL_TOP_LEVEL ON)
endif()
# Preserve existing cache switches for callers using the original options.
if(CIAL_TOP_LEVEL AND DEFINED ENABLE_TESTS AND NOT DEFINED CIAL_BUILD_TESTS)
    set(CIAL_BUILD_TESTS "${ENABLE_TESTS}" CACHE BOOL "Build tests")
endif()
if(CIAL_TOP_LEVEL AND DEFINED BUILD_EXECUTABLE AND NOT DEFINED CIAL_BUILD_CLI)
    set(CIAL_BUILD_CLI "${BUILD_EXECUTABLE}" CACHE BOOL "Build the command-line application")
endif()
if(CIAL_TOP_LEVEL AND DEFINED ENABLE_FUZZ AND NOT DEFINED CIAL_BUILD_FUZZ)
    set(CIAL_BUILD_FUZZ "${ENABLE_FUZZ}" CACHE BOOL "Build libFuzzer targets")
endif()
option(CIAL_BUILD_TESTS "Build tests" ${CIAL_TOP_LEVEL})
option(CIAL_BUILD_CLI "Build the command-line application" ${CIAL_TOP_LEVEL})
option(CIAL_BUILD_FORMATTER "Build the cial-fmt source formatter" ${CIAL_BUILD_CLI})
option(CIAL_BUILD_FUZZ "Build libFuzzer targets" OFF)
option(CIAL_BUILD_BENCH "Build benchmark and TJS2 conformance tooling" OFF)
option(CIAL_ENABLE_JIT "Enable the optional x64 integer JIT backend" OFF)
option(CIAL_BUILD_LSP "Build the cial-lsp skeleton executable" OFF)
option(CIAL_BUILD_DISASM "Build the cial-disasm TAC disassembler" OFF)
option(CIAL_BUILD_DBG "Build the cial-dbg skeleton executable" OFF)
option(CIAL_BUILD_TOOLS "Build all auxiliary tools (lsp/dbg skeletons, disasm, cial-fmt)" OFF)
if(CIAL_BUILD_TOOLS)
    # 一键全开：只影响本次配置，不落缓存
    set(CIAL_BUILD_LSP ON)
    set(CIAL_BUILD_DISASM ON)
    set(CIAL_BUILD_DBG ON)
    set(CIAL_BUILD_FORMATTER ON)
endif()
option(CIAL_WARNINGS_AS_ERRORS "Treat Ciallang compiler warnings as errors" ${CIAL_TOP_LEVEL})
foreach(feature IN ITEMS cli tests jit formatter tools)
    if((feature STREQUAL "cli" AND CIAL_BUILD_CLI) OR
       (feature STREQUAL "tests" AND CIAL_BUILD_TESTS) OR
       (feature STREQUAL "jit" AND CIAL_ENABLE_JIT) OR
       (feature STREQUAL "formatter" AND CIAL_BUILD_FORMATTER) OR
       (feature STREQUAL "tools" AND
           (CIAL_BUILD_LSP OR CIAL_BUILD_DISASM OR CIAL_BUILD_DBG)))
        list(APPEND VCPKG_MANIFEST_FEATURES ${feature})
    endif()
endforeach()
list(REMOVE_DUPLICATES VCPKG_MANIFEST_FEATURES)

# Version metadata is generated before project(); Python is a source-build dependency only.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_version.py"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/cial/Version.hpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_version.py"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json"
        --output-dir "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND_ERROR_IS_FATAL ANY)
include("${CMAKE_CURRENT_BINARY_DIR}/generated/Version.cmake")
project(Ciallang VERSION ${CIAL_VERSION} LANGUAGES CXX)
# 能力查询头：来自构建开关（doc/roadmap.md §3.1）；在 option() 处理之后生成。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Capability.hpp.in")
set(CIAL_CAP_JIT 0)
if(CIAL_ENABLE_JIT)
    set(CIAL_CAP_JIT 1)
endif()
configure_file(cmake/Capability.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/cial/Capability.hpp" @ONLY)
option(CIAL_ENABLE_PACKAGING "Enable standalone CLI archive packaging" OFF)
option(CIAL_INSTALL_SDK "Install the C++ SDK and CMake package" ${CIAL_TOP_LEVEL})

if(CIAL_BUILD_BENCH AND NOT CIAL_BUILD_CLI)
    message(FATAL_ERROR "CIAL_BUILD_BENCH requires CIAL_BUILD_CLI=ON")
endif()
if(CIAL_ENABLE_JIT)
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64)$")
        message(FATAL_ERROR "CIAL_ENABLE_JIT currently requires x64")
    endif()
    find_package(asmjit CONFIG REQUIRED)
endif()

# Reject stale Release caches that silently disable optimization.
if(CIAL_TOP_LEVEL AND CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_CXX_FLAGS_RELEASE MATCHES "(^| )(-O[1-3sz]|/O[12x])($| )")
    message(FATAL_ERROR "Release requires optimization flags; clear CMAKE_CXX_FLAGS_RELEASE and reconfigure")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(frozen CONFIG REQUIRED)
find_package(oniguruma CONFIG QUIET)
if(TARGET oniguruma::onig)
    set(CIAL_ONIG_TARGET oniguruma::onig)
    set(CIAL_ONIG_PACKAGE oniguruma)
else()
    find_package(unofficial-oniguruma CONFIG REQUIRED)
    set(CIAL_ONIG_TARGET unofficial::oniguruma::onig)
    set(CIAL_ONIG_PACKAGE unofficial-oniguruma)
endif()
set(CIAL_UTF8PROC_PACKAGE utf8proc)
find_package(utf8proc CONFIG QUIET)
if(NOT TARGET utf8proc AND NOT TARGET utf8proc::utf8proc)
    find_package(unofficial-utf8proc CONFIG REQUIRED)
    # vcpkg 的 unofficial 包也可能导出未加命名空间的 utf8proc 目标。
    # SDK 必须记录实际加载的包名，不能从目标名反推。
    set(CIAL_UTF8PROC_PACKAGE unofficial-utf8proc)
endif()
if(TARGET utf8proc)
    set(CIAL_UTF8PROC_TARGET utf8proc)
elseif(TARGET utf8proc::utf8proc)
    set(CIAL_UTF8PROC_TARGET utf8proc::utf8proc)
else()
    set(CIAL_UTF8PROC_TARGET unofficial::utf8proc::utf8proc)
endif()

set(SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/PreProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/Lexer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/Parser.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/SourceLocation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/SourceFile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/Colorizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/UTF8.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/UTF16.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/IRGenerator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/ControlFlowGraph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/SSA.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/Optimizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/Instruction.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Runtime.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/AtomTable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/gc.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Chunk.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Bytecode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/VMState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/VM.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/IntegerJit.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Rlc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Constant.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Class.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Function.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Octet.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Value.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/String.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/TypeConverter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Array.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Dictionary.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/RegExp.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/NativeRegister.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/StandardLibrary.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/Debug.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/StringLib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/ArrayLib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/DictLib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/RegExpLib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/MathLib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/DateLib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/DateParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/RandomLib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/SerializeLib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/ScriptsLib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tjsbc/TjsBytecodeReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tjsbc/TjsTranslator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/decompile/Decompiler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/decompile/ExprBuilder.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/Logger.cpp
)


add_library(Ciallang ${SOURCE_FILES})
add_library(Ciallang::Ciallang ALIAS Ciallang)
target_compile_features(Ciallang PUBLIC cxx_std_20)
set_target_properties(Ciallang PROPERTIES
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
include(GNUInstallDirs)
target_include_directories(Ciallang PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ciallang>")
target_link_libraries(Ciallang PUBLIC fmt::fmt frozen::frozen ${CIAL_UTF8PROC_TARGET} ${CIAL_ONIG_TARGET})
if(MSVC)
    target_compile_options(Ciallang PRIVATE /EHsc /MP /utf-8)
endif()
if(CIAL_ENABLE_JIT)
    target_compile_definitions(Ciallang PUBLIC CIAL_ENABLE_JIT=1)
    target_link_libraries(Ciallang PUBLIC asmjit::asmjit)
endif()

# cial 与 cial-fmt 均用 argparse 解析命令行；formatter-only 构建同样需要。
if(CIAL_BUILD_CLI OR CIAL_BUILD_FORMATTER)
    find_package(argparse CONFIG REQUIRED)
endif()

if(CIAL_BUILD_CLI)
    # 工具共享的编译编排与诊断渲染；main.cpp 与 Repl 留给 cial。
    # 生成的 Version.hpp 只被 Options.cpp（parseOptions --version）引用
    add_library(cialcli STATIC
        src/cli/Compilation.cpp
        src/cli/Diagnostics.cpp
        src/cli/Session.cpp)
    target_compile_features(cialcli PUBLIC cxx_std_20)
    target_link_libraries(cialcli PUBLIC Ciallang::Ciallang)
    target_include_directories(cialcli PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>")
    set_target_properties(cialcli PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
    if(MSVC)
        target_compile_options(cialcli PRIVATE /EHsc /MP /utf-8)
    endif()

    add_executable(cial
        src/main.cpp
        src/cli/Options.cpp
        src/cli/Application.cpp
        src/cli/Repl.cpp)
    target_link_libraries(cial PRIVATE Ciallang::Ciallang cialcli argparse::argparse)
    target_include_directories(cial PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set_target_properties(cial PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
    if(MSVC)
        target_compile_options(cial PRIVATE /EHsc /utf-8)
    endif()
endif()

# 源码格式化：词素扫描、布局与安全写回以 CiallangFormat 库形态放在 src/format/，
# exe 入口在 tools/fmt；formatter-only 独立于运行库/SDK，不创建 cial。
if(CIAL_BUILD_FORMATTER)
    add_library(CiallangFormat STATIC
        src/format/Formatter.cpp
        src/format/Files.cpp)
    target_link_libraries(CiallangFormat PUBLIC Ciallang::Ciallang)
    target_include_directories(CiallangFormat PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
    set_target_properties(CiallangFormat PROPERTIES CXX_EXTENSIONS OFF
        COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
    if(MSVC)
        target_compile_options(CiallangFormat PRIVATE /EHsc /utf-8)
    endif()
    add_subdirectory(tools/fmt)
    install(TARGETS cial-fmt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
endif()

# ------------------------------ auxiliary tools ------------------------------ #
# 各工具目录内只保留 exe 入口（main.cpp）；功能实现以库形态放在 src/ 下供外部复用：
# CiallangFormat（src/format，formatter-only 可用）与 cialdbg（src/dbg）。
# 各工具 skeleton 依赖尚未具备时以桩入口占位；共享的编译编排与诊断渲染见 cialcli。
if(CIAL_BUILD_DBG)
    add_library(cialdbg STATIC src/dbg/DebugSession.cpp)
    target_link_libraries(cialdbg PUBLIC Ciallang::Ciallang)
    target_include_directories(cialdbg PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
    set_target_properties(cialdbg PROPERTIES CXX_EXTENSIONS OFF
        COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
    if(MSVC)
        target_compile_options(cialdbg PRIVATE /EHsc /utf-8)
    endif()
endif()
set(CIAL_TOOL_TARGETS "")
foreach(tool IN ITEMS LSP DISASM DBG)
    if(CIAL_BUILD_${tool})
        string(TOLOWER "${tool}" CIAL_TOOL_DIR)
        if(NOT CIAL_BUILD_CLI)
            message(FATAL_ERROR "CIAL_BUILD_${tool} requires CIAL_BUILD_CLI=ON (shares cialcli)")
        endif()
        add_subdirectory(tools/${CIAL_TOOL_DIR})
        list(APPEND CIAL_TOOL_TARGETS cial-${CIAL_TOOL_DIR})
    endif()
endforeach()
if(CIAL_BUILD_FORMATTER)
    list(APPEND CIAL_TOOL_TARGETS cial-fmt)
endif()
if(CIAL_TOOL_TARGETS)
    add_custom_target(tools DEPENDS ${CIAL_TOOL_TARGETS}
        COMMENT "Build all enabled auxiliary tools")
endif()

if(CIAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ------------------------------ bench & conformance ------------------------------ #
if(CIAL_BUILD_BENCH)
    # TJSCompiler 参考实现定位: CIAL_TJS_ROOT 环境变量(推荐经 UserPresets 提供)
    # 或兄弟目录约定;可执行文件可用 CIAL_TJS_EXE 缓存变量强制指定
    set(TJS_ROOT "$ENV{CIAL_TJS_ROOT}" CACHE STRING "TJSCompiler 参考实现根目录")
    if(NOT TJS_ROOT)
        set(TJS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../TJSCompiler")
    endif()
    get_filename_component(TJS_ROOT "${TJS_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(CIAL_TJS_EXE "" CACHE STRING "TJSCompiler 可执行文件(留空则按 TJS_ROOT 探测)")
    if(CIAL_TJS_EXE)
        get_filename_component(CIAL_TJS_EXE "${CIAL_TJS_EXE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        foreach(candidate
                "${TJS_ROOT}/build/${CMAKE_SYSTEM_NAME}/${CMAKE_BUILD_TYPE}/TJSCompiler${CMAKE_EXECUTABLE_SUFFIX}"
                "${TJS_ROOT}/build/macos/TJSCompiler${CMAKE_EXECUTABLE_SUFFIX}"
                "${TJS_ROOT}/build/linux/TJSCompiler${CMAKE_EXECUTABLE_SUFFIX}"
                "${TJS_ROOT}/build/windows/TJSCompiler.exe"
                "${TJS_ROOT}/bin/Windows/TJSCompiler-release.exe"
                "${TJS_ROOT}/bin/Windows/TJSCompiler.exe")
            if(EXISTS "${candidate}")
                set(CIAL_TJS_EXE "${candidate}")
                break()
            endif()
        endforeach()
    endif()

    add_custom_target(bench
        COMMAND "${CMAKE_COMMAND}"
            -DCIAL_EXE=$<TARGET_FILE:cial>
            -DTJS_EXE=${CIAL_TJS_EXE}
            -DBENCH_DIR=${CMAKE_CURRENT_SOURCE_DIR}/bench
            -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Bench.cmake
        DEPENDS cial
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "运行基准(交替对拍,报告见 bench/results/)")

    add_custom_target(conformance-report
        COMMAND "${CMAKE_COMMAND}"
            -DCIAL_EXE=$<TARGET_FILE:cial>
            -DTJS_EXE=${CIAL_TJS_EXE}
            -DCORPUS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/test_files/tjs2
            -DREPORT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/bench/results/conformance.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Conformance.cmake
        DEPENDS cial
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "生成 TJS2 兼容性对拍报告(bench/results/conformance.txt)")
endif ()

if(CIAL_BUILD_FUZZ)
    add_subdirectory(fuzz)
endif ()

if(CIAL_INSTALL_SDK)
    include(cmake/SDK.cmake)
endif()

if(CIAL_ENABLE_PACKAGING)
    if(NOT CIAL_TOP_LEVEL OR NOT CIAL_BUILD_CLI)
        message(FATAL_ERROR "CLI packaging requires a standalone build with CIAL_BUILD_CLI=ON")
    endif()
    include(cmake/Packaging.cmake)
endif()
