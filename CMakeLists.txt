cmake_minimum_required(VERSION 3.25)

project(Ciallang CXX)



set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

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

        src/gen/BytecodeGen.cpp


        # ############################################
        src/vm/Interpreter.cpp
        src/vm/Chunk.cpp
        src/vm/Rlc.cpp
        src/vm/Instruction.cpp
        src/vm/Register.hpp
        src/vm/Label.hpp

        src/types/TjsNativeFunction.hpp
        src/types/TjsFunction.hpp
        src/types/TjsOctet.cpp
        src/types/TjsObject.cpp
        src/types/TjsValue.cpp
        src/types/TjsTypes.hpp

        src/init/GlogInit.hpp

        src/core/print.hpp

        src/gc/MarkSweep.cpp
        src/gc/Generational.cpp
        src/gc/GC.hpp
)
add_executable(${PROJECT_NAME}
        src/main.cpp
        ${SOURCE_FILES}
)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PUBLIC "/ZI")
  target_link_options(${PROJECT_NAME} PUBLIC "/INCREMENTAL")
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)
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