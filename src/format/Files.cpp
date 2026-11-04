#include "Files.hpp"

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

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

    void replaceFile(const std::filesystem::path &path, std::string_view original, std::string_view formatted) {
        if(fs::symlink_status(path).type() != fs::file_type::regular)
            throw std::runtime_error("refusing to replace a non-regular file (symbolic link)");
        if(readFile(path) != original)
            throw std::runtime_error("file changed since it was read");
        std::random_device random;
        fs::path temporary;
        int descriptor = -1;
        for(int attempt = 0; attempt < 32; ++attempt) {
            temporary = path;
            temporary += ".cial-fmt-" + std::to_string(random()) + ".tmp";
#ifdef _WIN32
            const auto status = _wsopen_s(&descriptor, temporary.c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY,
                                          _SH_DENYRW, _S_IREAD | _S_IWRITE);
            if(status != 0)
                errno = status;
#else
            descriptor = ::open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
#endif
            if(descriptor >= 0)
                break;
            if(errno != EEXIST)
                throw std::runtime_error(std::error_code(errno, std::generic_category()).message());
        }
        if(descriptor < 0)
            throw std::runtime_error("cannot create temporary file");
        auto closeFile = [&] {
#ifdef _WIN32
            return _close(descriptor);
#else
            return ::close(descriptor);
#endif
        };
        try {
            size_t position = 0;
            while(position < formatted.size()) {
                const auto count = std::min<size_t>(formatted.size() - position, 1024 * 1024);
#ifdef _WIN32
                const auto written = _write(descriptor, formatted.data() + position, static_cast<unsigned>(count));
#else
                const auto written = ::write(descriptor, formatted.data() + position, count);
#endif
                if(written < 0 && errno == EINTR)
                    continue;
                if(written <= 0)
                    throw std::runtime_error("cannot write temporary file");
                position += static_cast<size_t>(written);
            }
            const auto closed = closeFile();
            descriptor = -1;
            if(closed != 0)
                throw std::runtime_error("cannot close temporary file");
            fs::permissions(temporary, fs::status(path).permissions());
#ifdef _WIN32
            if(!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("cannot replace source file (Windows error " + std::to_string(GetLastError()) +
                                         ")");
#else
            fs::rename(temporary, path);
#endif
        } catch(...) {
            if(descriptor >= 0)
                closeFile();
            std::error_code ignored;
            fs::permissions(temporary, fs::perms::owner_write, fs::perm_options::add, ignored);
            fs::remove(temporary, ignored);
            throw;
        }
    }
} // namespace cial::format
