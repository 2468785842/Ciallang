cmake_minimum_required(VERSION 3.25)

project(Ciallang CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

add_executable(${PROJECT_NAME}
    src/main.cpp

    src/lexer/Lexer.cpp
    src/lexer/Token.hpp
    src/lexer/IEEETypes.hpp

    src/common/Bytes.hpp
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

    src/compiler/Compiler.cpp

    # ############################################
    src/vm/CiallangVM.cpp
    src/vm/VMTypes.cpp

    src/types/TjsOctet.cpp
    src/types/TjsString.cpp
    src/types/TjsObject.cpp
    src/types/TjsValue.cpp
    src/types/TjsTypes.hpp

)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    src/common
    src/lexer
    src/compiler
    src/parser
    src/ast
    src/vm
    src/types
)

include(depend)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt utf8proc)

# vscode clangd
# execute_process(
# COMMAND ${CMAKE_COMMAND} -E copy_if_different
# "${CMAKE_BINARY_DIR}/compile_commands.json"
# "${CMAKE_SOURCE_DIR}/build/compile_commands.json"
# )