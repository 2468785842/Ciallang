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
option(CIAL_BUILD_FUZZ "Build libFuzzer targets" OFF)
option(CIAL_BUILD_BENCH "Build benchmark and TJS2 conformance tooling" OFF)
option(CIAL_ENABLE_JIT "Enable the optional x64 integer JIT backend" OFF)
option(CIAL_WARNINGS_AS_ERRORS "Treat Ciallang compiler warnings as errors" ${CIAL_TOP_LEVEL})
foreach(feature IN ITEMS cli tests jit)
    if((feature STREQUAL "cli" AND CIAL_BUILD_CLI) OR
       (feature STREQUAL "tests" AND CIAL_BUILD_TESTS) OR
       (feature STREQUAL "jit" AND CIAL_ENABLE_JIT))
        list(APPEND VCPKG_MANIFEST_FEATURES ${feature})
    endif()
endforeach()
list(REMOVE_DUPLICATES VCPKG_MANIFEST_FEATURES)

project(Ciallang LANGUAGES CXX)

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
find_package(utf8proc CONFIG QUIET)
if(NOT TARGET utf8proc AND NOT TARGET utf8proc::utf8proc)
    find_package(unofficial-utf8proc CONFIG REQUIRED)
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

        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/IRGenerator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/ControlFlowGraph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/SSA.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/Optimizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gen/Instruction.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Runtime.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/AtomTable.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Chunk.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Bytecode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/VMState.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tjsbc/TjsBytecodeReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tjsbc/TjsTranslator.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gc.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/Logger.cpp
)


add_library(Ciallang ${SOURCE_FILES})
add_library(Ciallang::Ciallang ALIAS Ciallang)
target_compile_features(Ciallang PUBLIC cxx_std_20)
set_target_properties(Ciallang PROPERTIES
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
target_include_directories(Ciallang PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
target_link_libraries(Ciallang PUBLIC fmt::fmt frozen::frozen ${CIAL_UTF8PROC_TARGET})
if(MSVC)
    target_compile_options(Ciallang PRIVATE /EHsc /MP /utf-8)
endif()
if(CIAL_ENABLE_JIT)
    target_compile_definitions(Ciallang PUBLIC CIAL_ENABLE_JIT=1)
    target_link_libraries(Ciallang PUBLIC asmjit::asmjit)
endif()

if(CIAL_BUILD_CLI)
    find_package(argparse CONFIG REQUIRED)
    add_executable(cial
        src/main.cpp
        src/cli/Options.cpp
        src/cli/Application.cpp
        src/cli/Compilation.cpp
        src/cli/Diagnostics.cpp
        src/cli/Repl.cpp)
    target_link_libraries(cial PRIVATE Ciallang::Ciallang argparse::argparse)
    set_target_properties(cial PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
    if(MSVC)
        target_compile_options(cial PRIVATE /EHsc /utf-8)
    endif()
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
