// Continuous Engine - file IO helpers + directory watcher.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace cn::io {

CN_API std::optional<std::vector<u8>> read_file_bytes(const std::filesystem::path& p);
CN_API std::optional<std::string>     read_file_text (const std::filesystem::path& p);

CN_API bool write_file_bytes(const std::filesystem::path& p, const void* data, usize n);
CN_API bool write_file_text (const std::filesystem::path& p, std::string_view text);

CN_API std::filesystem::path executable_dir();
CN_API std::filesystem::path engine_root(); // located by walking up looking for vcpkg.json

// ----------------------------------------------------------------------------
// Directory watcher: posts callbacks on a worker thread when files in a tree
// change. Used for asset hot reload + gameplay DLL reload trigger.
// ----------------------------------------------------------------------------
struct ChangeEvent {
    std::filesystem::path path;
    enum class Kind { Created, Modified, Deleted, Renamed } kind{Kind::Modified};
};

class CN_API DirectoryWatcher {
public:
    DirectoryWatcher() = default;
    ~DirectoryWatcher();
    CN_NONCOPYABLE(DirectoryWatcher);

    using Callback = std::function<void(const ChangeEvent&)>;

    bool start(const std::filesystem::path& root, Callback cb);
    void stop();
    bool running() const { return running_; }

private:
    std::filesystem::path root_;
    Callback              cb_;
    std::thread           thread_;
    void*                 dir_handle_ = nullptr; // HANDLE
    bool                  running_    = false;
};

} // namespace cn::io
