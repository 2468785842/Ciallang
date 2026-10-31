#pragma once

#include <string>

#include "common/Result.hpp"
#include "vm/VMState.hpp"

namespace cial::cli {
    // 终端渲染：输出 file:line:col 前缀、错误码与源码摘录（details），无 TTY 时无颜色
    void printDiagnostics(const Common::Result &diagnostics);

    // 运行期错误：VM 只传播异常值，文件与位置（如有）由此处附加；
    // 无源码身份（字节码产物）时不伪造位置
    void printRuntimeError(const vm::VMState &state, const std::string &message);

    // 当前 JSON 诊断 schema 版本；旧版字段由 v2 派生
    constexpr int JsonSchemaVersion = 2;

    // schema 2：范围列为半开（end 为排他字节列），含字节偏移 range 与源码身份；
    // schema 1：兼容旧消费者，输出旧字段形状（不含 schema 标记）。
    // sourceName 用于无诊断时也保留源码身份（如 stdin 编译通过）
    void printJsonDiagnostics(const Common::Result &diagnostics, int schemaVersion = JsonSchemaVersion,
                              const std::string &sourceName = {});
} // namespace cial::cli
