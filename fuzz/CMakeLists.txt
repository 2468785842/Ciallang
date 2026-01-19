cmake_minimum_required(VERSION 3.12)
project(CialFuzz C CXX)

add_executable(fuzz_lexer fuzz_lexer.cc)

target_link_libraries(fuzz_lexer
        PRIVATE
        Ciallang
)

target_link_libraries(fuzz_lexer
        PRIVATE
        Ciallang
)

target_compile_options(fuzz_lexer PRIVATE
        -fsanitize=fuzzer,address,undefined
        -g
        -O1)

target_link_options(fuzz_lexer PRIVATE -fsanitize=fuzzer,address,undefined)

add_executable(fuzz_parser fuzz_parser.cc)

target_link_libraries(fuzz_parser
        PRIVATE
        Ciallang
)

target_compile_options(fuzz_parser PRIVATE
        -fsanitize=fuzzer,address,undefined
        -g
        -O1)

target_link_options(fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)

set_target_properties(fuzz_lexer fuzz_parser PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")