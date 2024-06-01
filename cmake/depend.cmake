cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE) # 禁用测试构建
set(INSTALL_HEADERS ON CACHE BOOL "" FORCE)

set(INSTALL_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(INSTALL_STATIC_LIBS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG "10.2.1"
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/fmt
)

FetchContent_Declare(
        utf8proc
        GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
        GIT_TAG "v2.9.0"
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/utf8proc
)

FetchContent_Declare(
        frozen
        GIT_REPOSITORY https://github.com/serge-sans-paille/frozen.git
        GIT_TAG "1.1.1"
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/frozen
)

FetchContent_Declare(
        gtest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG "v1.14.0"
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/googletest
        OVERRIDE_FIND_PACKAGE
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# gflags
if (NOT EXISTS ${CMAKE_BINARY_DIR}/gflags_357.patch)
    File(DOWNLOAD
            https://github.com/gflags/gflags/pull/357.patch
            ${CMAKE_BINARY_DIR}/gflags_357.patch)
endif ()

FetchContent_Declare(
        gflags
        GIT_REPOSITORY https://github.com/gflags/gflags.git
        GIT_TAG "v2.2.2"
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/gflags
        PATCH_COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/hack/gflags/config.cmake.in
        ${CMAKE_SOURCE_DIR}/ext/gflags/cmake/config.cmake.in
        COMMAND
        git apply ${CMAKE_BINARY_DIR}/gflags_357.patch
        UPDATE_DISCONNECTED 1
)

FetchContent_GetProperties(gflags)
if (NOT gflags_POPULATE)
    FetchContent_Populate(gflags)
#    set(CMAKE_INSTALL_PREFIX ${gflags_BINARY_DIR})
    add_subdirectory(${gflags_SOURCE_DIR} ${gflags_BINARY_DIR} EXCLUDE_FROM_ALL)
    set(gflags_DIR ${gflags_BINARY_DIR} CACHE STRING "" FORCE)
endif ()

FetchContent_Declare(
        glog
        GIT_REPOSITORY https://github.com/google/glog.git
        GIT_TAG "v0.7.0"
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/glog
)

FetchContent_MakeAvailable(fmt utf8proc frozen gtest glog)

