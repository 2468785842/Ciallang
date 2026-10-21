# 基准编排脚本(纯 CMake 脚本模式,无外部依赖)
#
# 用法(通常经 CMakeLists 的 bench 目标调用):
#   cmake -DCIAL_EXE=<cial> [-DTJS_EXE=<tjs>] -DBENCH_DIR=<bench>
#         -DSRC_DIR=<repo> [-DROUNDS=5] [-DWARMUP=1] -P Bench.cmake
#
# 计时: cmake -E time 包装子进程,取 wall-time(含进程启动,双引擎口径一致);
#       轮内 TJS→Ciallang 背靠背交替执行,抵消同机负载漂移。
# 校验: 基准脚本自带 throw 自检,任一引擎退出码非零即整体失败。
# 统计: CMake 无浮点,秒字符串换算为整数微秒,报告 min/mean。

if(NOT DEFINED ROUNDS)
    set(ROUNDS 5)
endif()
if(NOT DEFINED WARMUP)
    set(WARMUP 1)
endif()
if(NOT DEFINED BENCH_RESULTS_DIR)
    set(BENCH_RESULTS_DIR "${BENCH_DIR}/results")
endif()

# ── 工具函数 ────────────────────────────────────────────────

# 秒字符串("0.061") → 整数微秒
function(sec_to_us sec out_var)
    string(REGEX MATCH "^([0-9]*)\\.?([0-9]*)$" _m "${sec}")
    set(int_part "${CMAKE_MATCH_1}")
    set(frac_part "${CMAKE_MATCH_2}")
    if(int_part STREQUAL "")
        set(int_part 0)
    endif()
    string(LENGTH "${frac_part}" flen)
    set(padded "${frac_part}")
    while(flen LESS 6)
        set(padded "${padded}0")
        string(LENGTH "${padded}" flen)
    endwhile()
    string(SUBSTRING "${padded}" 0 6 us_frac)
    math(EXPR us "${int_part} * 1000000 + ${us_frac}")
    set(${out_var} "${us}" PARENT_SCOPE)
endfunction()

# 微秒 → "X.XXX ms" 显示
function(us_to_ms us out_var)
    math(EXPR ms "${us} / 1000")
    math(EXPR rem "${us} % 1000")
    string(REPLACE "." "" rem "${rem}")
    set(${out_var} "${ms}.${rem} ms" PARENT_SCOPE)
endfunction()

# 执行一次计时运行,返回微秒与退出码
function(run_timed exe script out_us out_rc)
    set(_stdout "")
    set(_stderr "")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E time "${exe}" "${script}"
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
        TIMEOUT 300
    )
    string(CONCAT combined "${_stdout}" "\n" "${_stderr}")
    # CMake 3.31: "Elapsed time (seconds): 0.0015756"(stdout)
    if(combined MATCHES [[Elapsed time \(seconds\): ([0-9]*\.?[0-9]+)]])
        sec_to_us("${CMAKE_MATCH_1}" us)
    else()
        set(us -1)
    endif()
    set(${out_us} "${us}" PARENT_SCOPE)
    set(${out_rc} "${rc}" PARENT_SCOPE)
endfunction()

# ── 溯源信息 ────────────────────────────────────────────────
set(git_cial "")
execute_process(COMMAND git rev-parse --short HEAD
                WORKING_DIRECTORY "${SRC_DIR}"
                OUTPUT_VARIABLE _o OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _r)
if(_r EQUAL 0)
    set(git_cial "${_o}")
endif()

set(report_header "Ciallang 基准报告  ${git_cial}  rounds=${ROUNDS}")
message(STATUS "${report_header}")

# ── 逐脚本交替执行 ──────────────────────────────────────────
file(GLOB scripts RELATIVE "${BENCH_DIR}" "${BENCH_DIR}/*.tjs")
list(SORT scripts)

if(NOT scripts)
    message(FATAL_ERROR "Bench.cmake: ${BENCH_DIR} 下没有基准脚本")
endif()

if(TJS_EXE)
    if(NOT EXISTS "${TJS_EXE}")
        message(FATAL_ERROR "Bench.cmake: TJS_EXE 不存在: ${TJS_EXE}")
    endif()
    message(STATUS "Ciallang : ${CIAL_EXE}")
    message(STATUS "TJS2     : ${TJS_EXE}")
else()
    message(STATUS "Ciallang : ${CIAL_EXE}")
    message(STATUS "TJS2     : (未设置,仅测 Ciallang)")
endif()

set(failed 0)
set(report "")

foreach(script IN LISTS scripts)
    set(case_name "${script}")
    string(REGEX REPLACE "\\.tjs$" "" case_name "${case_name}")

    # 预热(不计入)
    if(WARMUP GREATER 0)
        execute_process(COMMAND "${CIAL_EXE}" "${BENCH_DIR}/${script}"
                        TIMEOUT 300 RESULT_VARIABLE rc_w OUTPUT_QUIET ERROR_QUIET)
        if(NOT rc_w EQUAL 0)
            message(FATAL_ERROR "bench/${script}: Ciallang 预热失败 rc=${rc_w}(自检 throw?)")
        endif()
        if(TJS_EXE)
            execute_process(COMMAND "${TJS_EXE}" "${BENCH_DIR}/${script}"
                            TIMEOUT 300 RESULT_VARIABLE rc_w OUTPUT_QUIET ERROR_QUIET)
            if(NOT rc_w EQUAL 0)
                message(FATAL_ERROR "bench/${script}: TJS2 预热失败 rc=${rc_w}")
            endif()
        endif()
    endif()

    set(cial_us_list "")
    set(tjs_us_list "")
    set(case_failed 0)

    foreach(r RANGE 1 ${ROUNDS})
        if(TJS_EXE)
            run_timed("${TJS_EXE}" "${BENCH_DIR}/${script}" us rc)
            if(NOT rc EQUAL 0)
                message(SEND_ERROR "bench/${script} 第${r}轮: TJS2 退出码 ${rc}")
                set(case_failed 1)
            endif()
            list(APPEND tjs_us_list ${us})
        endif()
        run_timed("${CIAL_EXE}" "${BENCH_DIR}/${script}" us rc)
        if(NOT rc EQUAL 0)
            message(SEND_ERROR "bench/${script} 第${r}轮: Ciallang 退出码 ${rc}")
            set(case_failed 1)
        endif()
        list(APPEND cial_us_list ${us})
    endforeach()

    if(case_failed)
        math(EXPR failed "${failed} + 1")
        string(APPEND report "${case_name}: FAILED\n")
        continue()
    endif()

    # 统计: min / mean(整数微秒)
    foreach(engine cial tjs)
        set(min_${engine} 999999999999)
        set(total_${engine} 0)
        set(count_${engine} 0)
        foreach(us IN LISTS ${engine}_us_list)
            if(us LESS min_${engine})
                set(min_${engine} ${us})
            endif()
            math(EXPR total_${engine} "${total_${engine}} + ${us}")
            math(EXPR count_${engine} "${count_${engine}} + 1")
        endforeach()
        if(count_${engine} GREATER 0)
            math(EXPR mean_${engine} "${total_${engine}} / ${count_${engine}}")
        else()
            set(mean_${engine} 0)
        endif()
    endforeach()

    us_to_ms("${min_cial}" min_cial_ms)
    us_to_ms("${mean_cial}" mean_cial_ms)
    if(TJS_EXE)
        us_to_ms("${min_tjs}" min_tjs_ms)
        us_to_ms("${mean_tjs}" mean_tjs_ms)
        if(min_tjs GREATER 0)
            # 比值 = cial/tjs,放大 100 倍取整数保两位小数
            math(EXPR ratio_x100 "${min_cial} * 100 / ${min_tjs}")
            set(ratio_str "${ratio_x100}%")
        else()
            set(ratio_str "-")
        endif()
        string(APPEND report
               "${case_name}: TJS min=${min_tjs_ms} mean=${mean_tjs_ms} | "
               "Ciallang min=${min_cial_ms} mean=${mean_cial_ms} | Ciallang/TJS=${ratio_str}\n")
        message(STATUS "${case_name}: TJS ${min_tjs_ms} / Ciallang ${min_cial_ms} (min)  比值 ${ratio_str}")
    else()
        string(APPEND report "${case_name}: Ciallang min=${min_cial_ms} mean=${mean_cial_ms}\n")
        message(STATUS "${case_name}: Ciallang ${min_cial_ms} (min)")
    endif()
endforeach()

# ── 落盘 ────────────────────────────────────────────────────
string(TIMESTAMP stamp "%Y%m%d-%H%M%S")
file(WRITE "${BENCH_RESULTS_DIR}/bench-${stamp}.txt" "${report_header}\n\n${report}")
message(STATUS "报告已写入 ${BENCH_RESULTS_DIR}/bench-${stamp}.txt")

if(failed GREATER 0)
    message(FATAL_ERROR "Bench.cmake: ${failed} 个基准失败")
endif()
