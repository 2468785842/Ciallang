# cial-disasm：字节码反汇编工具骨架（gen 指令 dump + tjsbc 读取器）
add_executable(cial-disasm main.cpp)
target_link_libraries(cial-disasm PRIVATE Ciallang::Ciallang cialcli)
set_target_properties(cial-disasm PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
if(MSVC)
    target_compile_options(cial-disasm PRIVATE /EHsc /utf-8)
endif()
