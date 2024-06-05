cmake_minimum_required(VERSION 3.25)

project(Ciallang CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

set(SOURCE_FILES
        src/lexer/Lexer.cpp
        src/lexer/Token.hpp
        src/lexer/IEEETypes.hpp

        src/common/ConstExpr.hpp
        src/common/SourceLocation.cpp
        src/common/Result.hpp
        src/common/ResultMessage.hpp
        src/common/Defer.hpp
        src/common/SourceFile.cpp
        src/common/Colorizer.cpp
        src/common/UTF8.cpp

        src/parser/Parser.cpp

        src/ast/Ast.cpp
        src/ast/AstFormatter.cpp

        src/ast/AstNode.hpp
        src/ast/ExprNode.hpp
        src/ast/StmtNode.hpp
        src/ast/DeclNode.hpp

        src/compiler/Compiler.cpp

        src/gen/CodeGen.cpp

        # ############################################
        src/vm/Interpreter.cpp
        src/vm/VMChunk.cpp
        src/vm/Rlc.cpp
        src/vm/Memory.hpp
        src/vm/Instruction.hpp
        src/vm/DataPool.hpp

        src/types/TjsOctet.cpp
        src/types/TjsString.cpp
        src/types/TjsObject.cpp
        src/types/TjsValue.cpp
        src/types/TjsTypes.hpp

        src/init/GlogInit.hpp
)
add_executable(${PROJECT_NAME}
        src/main.cpp
        ${SOURCE_FILES}
)

target_precompile_headers(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/pch.h")

include(cmake/depend.cmake)

find_package(fmt REQUIRED)
find_package(utf8proc REQUIRED)
find_package(frozen REQUIRED)
find_package(gflags REQUIRED)
find_package(glog REQUIRED)

target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        fmt utf8proc frozen gflags glog
)
set(BUILD_TESTING ON)
if (${BUILD_TESTING})
    add_subdirectory(tests)
endif ()