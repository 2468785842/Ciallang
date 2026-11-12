# 构建期导出系统声明（doc/declarations.md §3）：注册事实 → 按顶层符号拆分的
# .d.tjs 文件目录，供 LSP 与安装产物消费。参数由调用方提供：
#   CIAL_EXE       cial 可执行文件路径
#   CIAL_DECL_DIR  输出声明目录
#   CIAL_SCRIPT_BUILTINS 手写的 Exception 自举声明（Exception.d.tjs）源路径
if(NOT CIAL_EXE OR NOT CIAL_DECL_DIR OR NOT CIAL_SCRIPT_BUILTINS)
    message(FATAL_ERROR "GenerateApiDeclarations.cmake requires CIAL_EXE, CIAL_DECL_DIR and CIAL_SCRIPT_BUILTINS")
endif()

file(REMOVE_RECURSE "${CIAL_DECL_DIR}")
# execute_process 按原始字节落盘，不做换行转换（声明偏移与读取端一致）。
execute_process(COMMAND "${CIAL_EXE}" --api-decl --api-decl-dir "${CIAL_DECL_DIR}" RESULT_VARIABLE cial_result)
if(NOT cial_result EQUAL 0)
    message(FATAL_ERROR "cial --api-decl-dir failed with exit code ${cial_result}")
endif()
# 脚本自举的 Exception 由手写声明随系统声明一起分发。
file(COPY_FILE "${CIAL_SCRIPT_BUILTINS}" "${CIAL_DECL_DIR}/Exception.d.tjs")
file(GLOB cial_declaration_files LIST_DIRECTORIES false "${CIAL_DECL_DIR}/*.d.tjs")
list(LENGTH cial_declaration_files cial_declaration_count)
if(cial_declaration_count LESS 2)
    message(FATAL_ERROR "expected per-type declarations plus Exception.d.tjs in ${CIAL_DECL_DIR}")
endif()

execute_process(COMMAND "${CIAL_EXE}" --version OUTPUT_VARIABLE cial_decl_version
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set(cial_decl_catalog "{\"schemaVersion\":1,\"version\":\"${cial_decl_version}\",\"files\":[]}")
set(cial_decl_index 0)
foreach(cial_decl_file IN LISTS cial_declaration_files)
    get_filename_component(cial_decl_name "${cial_decl_file}" NAME)
    file(SHA256 "${cial_decl_file}" cial_decl_hash)
    string(JSON cial_decl_catalog SET "${cial_decl_catalog}" files ${cial_decl_index}
        "{\"path\":\"${cial_decl_name}\",\"sha256\":\"${cial_decl_hash}\"}")
    math(EXPR cial_decl_index "${cial_decl_index} + 1")
endforeach()
file(WRITE "${CIAL_DECL_DIR}/index.json" "${cial_decl_catalog}\n")
file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/../packaging/declarations/README.md" "${CIAL_DECL_DIR}/README.md")
