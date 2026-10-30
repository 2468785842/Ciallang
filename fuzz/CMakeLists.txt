if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "libFuzzer requires Clang compiler")
endif()

# Instrument the implementation as well as the entry points. Propagate the
# sanitizer runtime to other consumers built in this dedicated configuration.
target_compile_options(Ciallang PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-sanitize-recover=all -fno-omit-frame-pointer -g -O1)
target_link_options(Ciallang INTERFACE -fsanitize=address,undefined)

foreach(name IN ITEMS lexer parser)
    add_executable(fuzz_${name} fuzz_${name}.cc)
    target_link_libraries(fuzz_${name} PRIVATE Ciallang::Ciallang)
    target_compile_options(fuzz_${name} PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-sanitize-recover=all -fno-omit-frame-pointer -g -O1)
    target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address,undefined)
endforeach()
