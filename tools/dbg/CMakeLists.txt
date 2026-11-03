# cial-dbg：交互式调试器骨架（依赖 vm 调试接口）
add_executable(cial-dbg main.cpp)
target_link_libraries(cial-dbg PRIVATE Ciallang::Ciallang cialcli)
set_target_properties(cial-dbg PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
if(MSVC)
    target_compile_options(cial-dbg PRIVATE /EHsc /utf-8)
endif()
