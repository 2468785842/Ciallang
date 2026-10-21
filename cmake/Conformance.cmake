# TJS2 兼容性对拍报告(纯 CMake 脚本模式,报告型:不影响构建结果)
#
# 用法(通常经 CMakeLists 的 conformance-report 目标调用):
#   cmake -DCIAL_EXE=<cial> -DTJS_EXE=<tjs> -DCORPUS_DIR=<dir>
#         -DSRC_DIR=<repo> [-DTIMEOUT=15] -P Conformance.cmake
#
# 当前比较维度: 成功/失败(退出码)一致性。
# 完整 stdout 语义对拍需要双引擎统一输出函数(待 TJSCompiler 侧注册 println
# 或本项目实现 Debug.message 后升级,见 TODO)。
#
# 分类:
#   BOTH_PASS  双方都跑通(兼容)
#   BOTH_FAIL  双方都失败(多为依赖未实现特性,可忽略)
#   TJS_ONLY   仅参考实现通过 —— 兼容性缺口清单
#   CIAL_ONLY  仅本项目通过(可疑,通常为错误信息/退出码口径差异,需人工复核)

if(NOT DEFINED TIMEOUT)
    set(TIMEOUT 15)
endif()
if(NOT DEFINED REPORT_PATH)
    set(REPORT_PATH "${CORPUS_DIR}/../../../bench/results/conformance.txt")
endif()

if(NOT EXISTS "${TJS_EXE}")
    message(FATAL_ERROR "Conformance.cmake: TJS_EXE 不存在: ${TJS_EXE}")
endif()

file(GLOB cases RELATIVE "${CORPUS_DIR}" "${CORPUS_DIR}/*.tjs")
list(SORT cases)

set(n_both_pass 0)
set(n_both_fail 0)
set(n_tjs_only 0)
set(n_cial_only 0)
set(detail "")

foreach(case IN LISTS cases)
    string(REGEX REPLACE "\\.tjs$" "" case_name "${case}")
    execute_process(COMMAND "${CIAL_EXE}" "${CORPUS_DIR}/${case}"
                    TIMEOUT ${TIMEOUT} RESULT_VARIABLE rc_cial
                    OUTPUT_QUIET ERROR_QUIET)
    execute_process(COMMAND "${TJS_EXE}" "${CORPUS_DIR}/${case}"
                    TIMEOUT ${TIMEOUT} RESULT_VARIABLE rc_tjs
                    OUTPUT_QUIET ERROR_QUIET)

    if(rc_cial EQUAL 0 AND rc_tjs EQUAL 0)
        set(verdict BOTH_PASS)
        math(EXPR n_both_pass "${n_both_pass} + 1")
    elseif(NOT rc_cial EQUAL 0 AND NOT rc_tjs EQUAL 0)
        set(verdict BOTH_FAIL)
        math(EXPR n_both_fail "${n_both_fail} + 1")
    elseif(NOT rc_cial EQUAL 0)
        set(verdict "TJS_ONLY ")
        math(EXPR n_tjs_only "${n_tjs_only} + 1")
    else()
        set(verdict "CIAL_ONLY")
        math(EXPR n_cial_only "${n_cial_only} + 1")
    endif()
    string(APPEND detail "${verdict}  ${case}  (cial rc=${rc_cial}, tjs rc=${rc_tjs})\n")
endforeach()

string(TIMESTAMP stamp "%Y%m%d-%H%M%S")
file(WRITE "${REPORT_PATH}"
"TJS2 兼容性对拍报告  ${stamp}
语料: ${CORPUS_DIR}
分类统计: BOTH_PASS=${n_both_pass}  BOTH_FAIL=${n_both_fail}  TJS_ONLY=${n_tjs_only}  CIAL_ONLY=${n_cial_only}

${detail}")
message(STATUS "对拍统计: 兼容 ${n_both_pass} / 双失败 ${n_both_fail} / 缺口(TJS_ONLY) ${n_tjs_only} / 待复核 ${n_cial_only}")
message(STATUS "报告已写入 ${REPORT_PATH}")
