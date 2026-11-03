# cial-dbg：TJS2 交互式调试器（doc/debugger.md）
add_executable(cial-dbg
    main.cpp
    DebugSession.cpp)
target_link_libraries(cial-dbg PRIVATE Ciallang::Ciallang cialcli)
set_target_properties(cial-dbg PROPERTIES COMPILE_WARNING_AS_ERROR ${CIAL_WARNINGS_AS_ERRORS})
if(MSVC)
    target_compile_options(cial-dbg PRIVATE /EHsc /utf-8)
endif()
