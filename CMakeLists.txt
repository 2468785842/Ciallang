cmake_minimum_required(VERSION 3.25)

project(Ciallang CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/lexer/Lexer.cpp
        ${CMAKE_SOURCE_DIR}/src/lexer/Token.hpp
        ${CMAKE_SOURCE_DIR}/src/lexer/IEEETypes.hpp

        ${CMAKE_SOURCE_DIR}/src/common/Bytes.hpp
        ${CMAKE_SOURCE_DIR}/src/common/SourceLocation.cpp
        ${CMAKE_SOURCE_DIR}/src/common/Result.hpp
        ${CMAKE_SOURCE_DIR}/src/common/ResultMessage.hpp
        ${CMAKE_SOURCE_DIR}/src/common/Defer.hpp
        ${CMAKE_SOURCE_DIR}/src/common/SourceFile.cpp
        ${CMAKE_SOURCE_DIR}/src/common/Colorizer.cpp
        ${CMAKE_SOURCE_DIR}/src/common/UTF8.cpp

        ${CMAKE_SOURCE_DIR}/src/parser/Parser.cpp

        ${CMAKE_SOURCE_DIR}/src/ast/Ast.cpp
        ${CMAKE_SOURCE_DIR}/src/ast/AstFormatter.cpp

        ${CMAKE_SOURCE_DIR}/src/ast/AstNode.hpp
        ${CMAKE_SOURCE_DIR}/src/ast/ExprNode.hpp
        ${CMAKE_SOURCE_DIR}/src/ast/StmtNode.hpp

        ${CMAKE_SOURCE_DIR}/src/compiler/Compiler.cpp

        # ############################################
        ${CMAKE_SOURCE_DIR}/src/vm/CiallangVM.cpp
        ${CMAKE_SOURCE_DIR}/src/vm/VMChunk.cpp
        ${CMAKE_SOURCE_DIR}/src/vm/Rlc.cpp
        ${CMAKE_SOURCE_DIR}/src/vm/Memory.hpp
        ${CMAKE_SOURCE_DIR}/src/vm/Instruction.hpp
        ${CMAKE_SOURCE_DIR}/src/vm/DataPool.hpp

        ${CMAKE_SOURCE_DIR}/src/types/TjsOctet.cpp
        ${CMAKE_SOURCE_DIR}/src/types/TjsString.cpp
        ${CMAKE_SOURCE_DIR}/src/types/TjsObject.cpp
        ${CMAKE_SOURCE_DIR}/src/types/TjsValue.cpp
        ${CMAKE_SOURCE_DIR}/src/types/TjsTypes.hpp

)
add_executable(${PROJECT_NAME}
        src/main.cpp
        ${SOURCE_FILES}
)

include(depend)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt utf8proc frozen)

add_subdirectory(tests)