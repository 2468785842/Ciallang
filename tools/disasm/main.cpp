// cial-disasm 桩入口：反汇编目标依赖 Bytecode dump 与 tjsbc 读取器；
// 当前等价入口为 `cial --bytecode` 配合 CIAL_TJSBC_DUMP 环境变量。
#include <fmt/format.h>

int main() {
    fmt::println(stderr, "cial-disasm: not implemented yet; use `cial --bytecode` with CIAL_TJSBC_DUMP=1");
    return 2;
}
