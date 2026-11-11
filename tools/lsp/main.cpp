// cial-lsp：核心集 stdio 语言服务器（doc/lsp.md）。诊断、声明解析与类型推导
// 全部进程内完成；系统声明从可执行文件旁 declarations/ 或安装布局
// share/ciallang/declarations 读取，--declarations 可追加目录。
// stdout 仅承载带 Content-Length 的 JSON-RPC 帧；日志一律走 stderr。
#include <lsp/Server.hpp>

#include <exception>
#include <iostream>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

int main(const int argc, char **argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    // stdio 帧按原始字节传输：文本模式会把 \n 再译成 \r\n，破坏帧头与长度
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    std::vector<std::filesystem::path> directories;
    for(int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if(argument == "--declarations") {
            if(++i >= argc) {
                std::cerr << "cial-lsp: --declarations expects a directory\n";
                return 2;
            }
            directories.emplace_back(argv[i]);
        } else if(argument == "--help" || argument == "-h") {
            std::cout << "usage: cial-lsp [--declarations <dir>]\n";
            return 0;
        } else {
            std::cerr << "cial-lsp: unknown argument " << argument << '\n';
            return 2;
        }
    }
    try {
        cial::lsp::Server server{ std::move(directories) };
        return server.run(std::cin, std::cout);
    } catch(const std::exception &failure) {
        std::cerr << "cial-lsp: " << failure.what() << '\n';
        return 1;
    }
}
