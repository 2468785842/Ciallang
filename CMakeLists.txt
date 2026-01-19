cmake_minimum_required(VERSION 3.25)

project(Ciallang CXX)

option(ENABLE_FUZZ "enable fuzz execute build" OFF)
option(ENABLE_TESTS "enable tests execute build" ON)
option(BUILD_EXECUTABLE "build executable file" ON)

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

        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Runtime.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/AtomTable.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/VMState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Rlc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Instruction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/Constant.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Class.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Function.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Octet.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/Value.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/String.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types/TypeConverter.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/NativeRegister.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/Debug.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/StringLib.cpp

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
endif()

if (${ENABLE_TESTS})
    add_subdirectory(tests)
endif ()

if(${ENABLE_FUZZ})
    add_subdirectory(fuzz)
endif ()