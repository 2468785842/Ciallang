# TJS2 兼容性对拍报告(纯 CMake 脚本模式,报告型:不影响构建结果)
#
# 用法(通常经 CMakeLists 的 conformance-report 目标调用):
#   cmake -DCIAL_EXE=<cial> -DTJS_EXE=<tjs> -DCORPUS_DIR=<dir>
#         -DREPORT_PATH=<out> [-DTIMEOUT=15] -P Conformance.cmake
#
# 比较维度:退出码成败 + stdout 逐行语义对拍(带双端归一化):
#   Ciallang 侧: 剔除 "Time taken..." 计时行;字符串输出剥掉两端引号
#               (Value 的 ostream 输出对字符串加引号,TJS 不加)
#   TJS 侧:     剔除尾部的 "TJS Context Dump" 引擎转储段;
#               Debug.message 输出剥掉 "HH:MM:SS " 时间戳前缀
#   Ciallang main 注入的 Debug.message 垫片使同一脚本双端可原样运行
#
# 分类:
#   MATCH      双方成功且输出一致(真兼容)
#   DIFF       双方成功但输出不同(语义差异,最重要的一类)
#   TJS_ONLY   仅参考实现成功 —— 功能缺口
#   CIAL_ONLY  仅本项目成功(可疑,通常为错误信息/退出码口径差异)
#   BOTH_FAIL  双方都失败(多为依赖未实现特性,可忽略)

if(NOT DEFINED TIMEOUT)
    set(TIMEOUT 15)
endif()
if(NOT EXISTS "${TJS_EXE}")
    message(FATAL_ERROR "Conformance.cmake: TJS_EXE 不存在: ${TJS_EXE}")
endif()

# 归一化一行输出
function(normalize_line engine line out_var)
    set(l "${line}")
    if(engine STREQUAL "cial")
        # 剔除计时行
        if(l MATCHES "^Time taken by function:")
            set(${out_var} "" PARENT_SCOPE)
            return()
        endif()
        # "text" -> text(仅当整行被引号包裹)
        string(REGEX REPLACE "^\"(.*)\"$" "\\1" l "${l}")
    else()
        # HH:MM:SS 前缀 -> 剥除
        string(REGEX REPLACE "^[0-9][0-9]:[0-9][0-9]:[0-9][0-9] " "" l "${l}")
        # Context Dump 横幅行(全 # 组成)整行丢弃
        if(l MATCHES "^#+$")
            set(${out_var} "" PARENT_SCOPE)
            return()
        endif()
    endif()
    # 纯空白行丢弃(TJS 转储标题行是整行空格)
    if(l MATCHES "^ *$")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    set(${out_var} "${l}" PARENT_SCOPE)
endfunction()

# 输出文本 -> 归一化行列表(丢弃空行与 TJS 尾部转储段)
function(normalize_output engine text out_var)
    if(engine STREQUAL "tjs")
        # 截断引擎转储(Dump() 在脚本执行后输出)
        string(FIND "${text}" "TJS Context Dump" dump_pos)
        if(NOT dump_pos EQUAL -1)
            string(SUBSTRING "${text}" 0 ${dump_pos} text)
        endif()
    endif()
    string(REPLACE "\r" "" text "${text}")
    string(REPLACE "\n" ";" lines "${text}")
    set(result "")
    foreach(l IN LISTS lines)
        normalize_line("${engine}" "${l}" nl)
        if(NOT nl STREQUAL "")
            list(APPEND result "${nl}")
        endif()
    endforeach()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

file(GLOB cases RELATIVE "${CORPUS_DIR}" "${CORPUS_DIR}/*.tjs")
list(SORT cases)

set(n_match 0)
set(n_diff 0)
set(n_both_fail 0)
set(n_tjs_only 0)
set(n_cial_only 0)
set(detail "")

foreach(case IN LISTS cases)
    string(REGEX REPLACE "\\.tjs$" "" case_name "${case}")
    set(cial_out "")
    set(tjs_out "")
    execute_process(COMMAND "${CIAL_EXE}" "${CORPUS_DIR}/${case}"
                    TIMEOUT ${TIMEOUT} RESULT_VARIABLE rc_cial
                    OUTPUT_VARIABLE cial_out ERROR_QUIET)
    execute_process(COMMAND "${TJS_EXE}" "${CORPUS_DIR}/${case}"
                    TIMEOUT ${TIMEOUT} RESULT_VARIABLE rc_tjs
                    OUTPUT_VARIABLE tjs_out ERROR_QUIET)

    if(rc_cial EQUAL 0 AND rc_tjs EQUAL 0)
        normalize_output(cial "${cial_out}" cial_lines)
        normalize_output(tjs "${tjs_out}" tjs_lines)
        if(cial_lines STREQUAL tjs_lines)
            math(EXPR n_match "${n_match} + 1")
            string(APPEND detail "MATCH     ${case}\n")
        else()
            math(EXPR n_diff "${n_diff} + 1")
            # 找出首个差异行供快速定位
            set(diff_note "")
            list(LENGTH cial_lines cn)
            list(LENGTH tjs_lines tn)
            set(idx 0)
            set(maxidx "${cn}")
            if(tn LESS maxidx)
                set(maxidx "${tn}")
            endif()
            while(idx LESS maxidx)
                list(GET cial_lines ${idx} cl)
                list(GET tjs_lines ${idx} tl)
                if(NOT cl STREQUAL tl)
                    set(diff_note "  首个差异行 ${idx}: cial=[${cl}] tjs=[${tl}]")
                    break()
                endif()
                math(EXPR idx "${idx} + 1")
            endwhile()
            if(diff_note STREQUAL "")
                set(diff_note "  行数不同: cial=${cn} tjs=${tn}")
            endif()
            string(APPEND detail "DIFF      ${case}${diff_note}\n")
        endif()
    elseif(NOT rc_cial EQUAL 0 AND NOT rc_tjs EQUAL 0)
        math(EXPR n_both_fail "${n_both_fail} + 1")
        string(APPEND detail "BOTH_FAIL ${case}  (cial rc=${rc_cial}, tjs rc=${rc_tjs})\n")
    elseif(NOT rc_cial EQUAL 0)
        math(EXPR n_tjs_only "${n_tjs_only} + 1")
        string(APPEND detail "TJS_ONLY  ${case}  (cial rc=${rc_cial})\n")
    else()
        math(EXPR n_cial_only "${n_cial_only} + 1")
        string(APPEND detail "CIAL_ONLY ${case}  (tjs rc=${rc_tjs})\n")
    endif()
endforeach()

string(TIMESTAMP stamp "%Y%m%d-%H%M%S")
file(WRITE "${REPORT_PATH}"
"TJS2 兼容性对拍报告(stdout 语义)  ${stamp}
语料: ${CORPUS_DIR}
统计: MATCH=${n_match}  DIFF=${n_diff}  TJS_ONLY=${n_tjs_only}  CIAL_ONLY=${n_cial_only}  BOTH_FAIL=${n_both_fail}

${detail}")
message(STATUS "对拍: 一致 ${n_match} / 输出差异 ${n_diff} / 缺口 ${n_tjs_only} / 待复核 ${n_cial_only} / 双失败 ${n_both_fail}")
message(STATUS "报告已写入 ${REPORT_PATH}")
