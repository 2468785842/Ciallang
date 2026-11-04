# cial-fmt：TJS 源码格式化 CLI（doc/formatting.md）；词素扫描、布局与安全写回
# 实现在 CiallangFormat（src/format/），本目录只保留 exe 入口。
add_executable(cial-fmt main.cpp)
target_link_libraries(cial-fmt PRIVATE CiallangFormat argparse::argparse)
set_target_properties(cial-fmt PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
if(MSVC)
    target_compile_options(cial-fmt PRIVATE /EHsc /utf-8)
endif()
