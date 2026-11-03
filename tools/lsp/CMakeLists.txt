# cial-lsp：stdio LSP 服务骨架（协议原型 tools/lsp/cial_lsp.py，见 doc/ 索引）
add_executable(cial-lsp main.cpp)
target_link_libraries(cial-lsp PRIVATE Ciallang::Ciallang cialcli)
set_target_properties(cial-lsp PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
if(MSVC)
    target_compile_options(cial-lsp PRIVATE /EHsc /utf-8)
endif()
