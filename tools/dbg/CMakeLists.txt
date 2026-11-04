# cial-dbg：TJS2 交互式调试器（doc/debugger.md）；调试会话实现在 cialdbg（src/dbg/），
# 本目录只保留 exe 入口。
add_executable(cial-dbg main.cpp)
target_link_libraries(cial-dbg PRIVATE Ciallang::Ciallang cialcli cialdbg argparse::argparse)
set_target_properties(cial-dbg PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
if(MSVC)
    target_compile_options(cial-dbg PRIVATE /EHsc /utf-8)
endif()
