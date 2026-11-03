// cial-lsp 桩入口：协议原型见 tools/lsp/cial_lsp.py（经 cial 子进程驱动），
// 本可执行文件将逐步内化诊断与符号能力；落地前保持退出码 2。
#include <fmt/format.h>

int main() {
    fmt::println(stderr, "cial-lsp: not implemented yet; the stdio prototype lives in tools/lsp/cial_lsp.py");
    return 2;
}
