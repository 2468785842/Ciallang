cmake_minimum_required(VERSION 3.25)

project(Ciallang CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

include_directories("${CMAKE_SOURCE_DIR}/src")

add_executable(
        vm_instruction_test
        vm/instruction_test.cpp
        ${SOURCE_FILES}
)

target_link_libraries(
        vm_instruction_test
        PRIVATE
        GTest::gtest_main
        fmt utf8proc frozen gtest gflags glog
)

include(GoogleTest)
gtest_discover_tests(vm_instruction_test)
