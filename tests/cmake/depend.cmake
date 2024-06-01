
cmake_minimum_required(VERSION 3.25)

include(FetchContent)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE) # 禁用测试构建
set(INSTALL_HEADERS ON CACHE BOOL "" FORCE)

set(INSTALL_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(INSTALL_STATIC_LIBS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        gtest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG "v1.14.0"
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/googletest
        OVERRIDE_FIND_PACKAGE
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(gtest)
