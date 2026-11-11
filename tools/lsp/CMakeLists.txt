# cial-lsp：核心集 stdio 语言服务器（实现库 src/lsp/，见 doc/lsp.md）；
# 诊断与声明解析在进程内完成，不再派生 cial 子进程。
add_executable(cial-lsp main.cpp)
target_link_libraries(cial-lsp PRIVATE ciallsp)
set_target_properties(cial-lsp PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
if(MSVC)
    target_compile_options(cial-lsp PRIVATE /EHsc /utf-8)
endif()
