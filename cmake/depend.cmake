include(FetchContent)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG "10.2.1"
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/fmt
        BINARY_DIR ${CMAKE_BINARY_DIR}/_deps/fmt/build
        SUBBUILD_DIR ${CMAKE_BINARY_DIR}/_deps/fmt/subbuild
)

FetchContent_Declare(
        utf8proc
        GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
        GIT_TAG "v2.9.0"
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/utf8proc
        BINARY_DIR ${CMAKE_BINARY_DIR}/_deps/utf8proc/build
        SUBBUILD_DIR ${CMAKE_BINARY_DIR}/_deps/utf8proc/subbuild
)
FetchContent_MakeAvailable(fmt utf8proc)