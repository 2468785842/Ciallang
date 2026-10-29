cmake_minimum_required(VERSION 3.25)

option(CIAL_ENABLE_JIT "Enable the optional x64 integer JIT backend" OFF)
if(CIAL_ENABLE_JIT)
    list(APPEND VCPKG_MANIFEST_FEATURES jit)
    list(REMOVE_DUPLICATES VCPKG_MANIFEST_FEATURES)
endif()

project(Ciallang CXX)

if(CIAL_ENABLE_JIT)
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64)$")
        message(FATAL_ERROR "CIAL_ENABLE_JIT currently requires x64")
    endif()
    find_package(asmjit CONFIG REQUIRED)
endif()

option(ENABLE_FUZZ "enable fuzz execute build" OFF)
option(CIAL_BUILD_BENCH "build benchmark & TJS2 conformance tooling" OFF)
option(ENABLE_TESTS "enable tests execute build" ON)
option(BUILD_EXECUTABLE "build executable file" ON)

# Release 缓存污染会使性能测量失真；显式报错，避免静默以 -O0 运行。
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_CXX_FLAGS_RELEASE MATCHES "(^| )(-O[1-3sz]|/O[12x])($| )")
    message(FATAL_ERROR "Release 缺少优化标志，请清理 CMAKE_CXX_FLAGS_RELEASE 缓存后重新配置")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/EHsc>" "/MP" "/utf-8")
    add_link_options("/INCREMENTAL" "/DEBUG:FASTLINK")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(utf8proc CONFIG QUIET)
if(NOT TARGET utf8proc)
    find_package(unofficial-utf8proc CONFIG)
endif()
find_package(frozen CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
#target_precompile_headers(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h")

target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC
        fmt::fmt utf8proc frozen::frozen argparse::argparse spdlog::spdlog
)

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_WARNING_AS_ERROR ON)

if(${BUILD_EXECUTABLE})
    add_executable(cial
            src/main.cpp
            ${SOURCE_FILES}
    )

    set_target_properties(cial PROPERTIES COMPILE_WARNING_AS_ERROR ON)

#    target_precompile_headers(cial PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h")
    target_link_libraries(
            cial
            PRIVATE
            fmt::fmt utf8proc frozen::frozen argparse::argparse spdlog::spdlog
    )

    add_executable(cial_bc
            src/main0.cpp
            ${SOURCE_FILES}
    )

    set_target_properties(cial_bc PROPERTIES COMPILE_WARNING_AS_ERROR ON)

    #    target_precompile_headers(cial PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h")
    target_link_libraries(
            cial_bc
            PRIVATE
            fmt::fmt utf8proc frozen::frozen argparse::argparse spdlog::spdlog
    )
endif()

if(CIAL_ENABLE_JIT)
    foreach(target Ciallang cial cial_bc)
        if(TARGET ${target})
            target_compile_definitions(${target} PUBLIC CIAL_ENABLE_JIT=1)
            target_link_libraries(${target} PUBLIC asmjit::asmjit)
        endif()
    endforeach()
endif()

if (${ENABLE_TESTS})
    add_subdirectory(tests)
endif ()

# ------------------------------ bench & conformance ------------------------------ #
if (${CIAL_BUILD_BENCH})
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

if(${ENABLE_FUZZ})
    add_subdirectory(fuzz)
endif ()
