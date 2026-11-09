#include "Files.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <system_error>

namespace cial::format {
    namespace {
        namespace fs = std::filesystem;
    }

    std::string readFile(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        if(!stream)
            throw std::runtime_error("cannot open file");
        std::string contents{ std::istreambuf_iterator<char>(stream), {} };
        if(stream.bad())
            throw std::runtime_error("cannot read file");
        return contents;
    }

    // 原子写回：随机命名临时文件（独占探测）→ 写入 → 继承权限 → rename 覆盖。
    // 全部经 C++ 标准库（fs::rename 在 MSVC STL 上映射为覆盖式移动），不触碰
    // 平台 CRT，避免 CI 上 _O_* / _write / _close 一类的声明与弃用问题
    void replaceFile(const std::filesystem::path &path, std::string_view original, std::string_view formatted) {
        if(fs::symlink_status(path).type() != fs::file_type::regular)
            throw std::runtime_error("refusing to replace a non-regular file (symbolic link)");
        if(readFile(path) != original)
            throw std::runtime_error("file changed since it was read");
        std::random_device random;
        fs::path temporary;
        std::ofstream stream;
        for(int attempt = 0; attempt < 32; ++attempt) {
            temporary = path;
            temporary += ".cial-fmt-" + std::to_string(random()) + ".tmp";
            std::error_code existsError;
            if(fs::exists(temporary, existsError) && !existsError)
                continue; // 名字被占用（历史残留或并发实例），换一个
            stream.open(temporary, std::ios::binary | std::ios::out | std::ios::trunc);
            if(stream)
                break;
        }
        if(!stream)
            throw std::runtime_error("cannot create temporary file");
        try {
            stream.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
            if(!stream)
                throw std::runtime_error("cannot write temporary file");
            stream.close();
            if(stream.fail())
                throw std::runtime_error("cannot close temporary file");
            fs::permissions(temporary, fs::status(path).permissions());
            std::error_code renameError;
            fs::rename(temporary, path, renameError);
            if(renameError)
                throw std::runtime_error("cannot replace source file: " + renameError.message());
        } catch(...) {
            std::error_code ignored;
            if(stream.is_open())
                stream.close();
            fs::permissions(temporary, fs::perms::owner_write, fs::perm_options::add, ignored);
            fs::remove(temporary, ignored);
            throw;
        }
    }
} // namespace cial::format
