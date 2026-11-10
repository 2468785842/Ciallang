# cial-disasm：TJS2 字节码/自产源码的 TAC 反汇编工具（渲染走 decompile 库 disassemblyOnly）
add_executable(cial-disasm main.cpp)
target_link_libraries(cial-disasm PRIVATE Ciallang::Ciallang cialcli)
set_target_properties(cial-disasm PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
if(MSVC)
    target_compile_options(cial-disasm PRIVATE /EHsc /utf-8)
endif()
