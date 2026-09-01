#include "core/atomic_file.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>

bool atomicWriteFile(const std::filesystem::path& path, const std::string& content) {
    std::error_code error;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    const auto temporary = path.string() + ".tmp-" + std::to_string(getpid());
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file < 0) return false;
    size_t written = 0;
    bool ok = true;
    while (written < content.size()) {
        const auto count = write(file, content.data() + written, content.size() - written);
        if (count > 0)
            written += static_cast<size_t>(count);
        else if (count < 0 && errno == EINTR)
            continue;
        else {
            ok = false;
            break;
        }
    }
    if (ok) ok = fsync(file) == 0;
    if (close(file) != 0) ok = false;
    if (ok) ok = rename(temporary.c_str(), path.c_str()) == 0;
    if (!ok) unlink(temporary.c_str());
    return ok;
}
