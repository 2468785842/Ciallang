// cial-dbg 桩入口：交互式调试器依赖 VM 调试接口暴露，落地前保持退出码 2。
// 注意：脚本内 Debug 类（src/stdlib/Debug.cpp）属于引擎，与本工具无关。
#include <fmt/format.h>

int main() {
    fmt::println(stderr, "cial-dbg: not implemented yet");
    return 2;
}
