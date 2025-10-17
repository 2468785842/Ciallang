cmake_minimum_required(VERSION 3.25)

project(Ciallang CXX)

option(ENABLE_TESTS "enable tests execute build" ON)
option(BUILD_EXECUTABLE "build executable file" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")

if(MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/EHsc>" "/MP" "/utf-8")
    add_link_options("/INCREMENTAL" "/DEBUG:FASTLINK")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(utf8proc CONFIG)
if(NOT TARGET utf8proc)
    find_package(unofficial-utf8proc CONFIG)
endif()
find_package(frozen CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(SOURCE_FILES
        src/lexer/Lexer.cpp
        src/lexer/Token.hpp
        src/lexer/IEEETypes.hpp

        src/common/SourceLocation.cpp
        src/common/Result.hpp
        src/common/ResultMessage.hpp
        src/common/Defer.hpp
        src/common/SourceFile.cpp
        src/common/Colorizer.cpp
        src/common/UTF8.cpp
        src/common/TableFormatter.hpp

        src/parser/Parser.cpp

        src/ast/Ast.cpp

        src/ast/AstNode.hpp
        src/ast/ExprNode.hpp
        src/ast/StmtNode.hpp
        src/ast/DeclNode.hpp
        src/ast/AstFormatter.hpp

        src/gen/IRGenerator.cpp

        # ############################################
        src/vm/VMState.cpp
        src/vm/Rlc.cpp
        src/vm/Instruction.cpp
        src/vm/Register.hpp
        src/vm/Label.hpp

        src/types/Function.cpp
        src/types/TjsOctet.cpp
        src/types/TjsObject.cpp
        src/types/TjsValue.cpp
        src/types/TjsString.cpp
        src/types/TjsTypes.hpp

        src/standard/Debug.cpp

        src/gc/GC.cpp

        src/logging/Logger.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
#target_precompile_headers(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/pch.h")

target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        fmt::fmt utf8proc frozen::frozen argparse::argparse spdlog::spdlog
)

if(${BUILD_EXECUTABLE})
    add_executable(cll
            src/main.cpp
            ${SOURCE_FILES}
    )
#    target_precompile_headers(cll PRIVATE "${CMAKE_SOURCE_DIR}/src/pch.h")
    target_link_libraries(
            cll
            PRIVATE
            fmt::fmt utf8proc frozen::frozen argparse::argparse spdlog::spdlog
    )
endif()

if (${ENABLE_TESTS} AND ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_subdirectory(tests)
endif ()
