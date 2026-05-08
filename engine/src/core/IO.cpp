#include "continuous/core/IO.h"
#include "continuous/core/Log.h"

#include <fstream>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace cn::io {

std::optional<std::vector<u8>> read_file_bytes(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return std::nullopt;
    auto sz = f.tellg();
    if (sz < 0) return std::nullopt;
    std::vector<u8> buf(static_cast<usize>(sz));
    f.seekg(0);
    if (!f.read(reinterpret_cast<char*>(buf.data()), buf.size())) return std::nullopt;
    return buf;
}

std::optional<std::string> read_file_text(const std::filesystem::path& p) {
    std::ifstream f(p);
    if (!f) return std::nullopt;
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return s;
}

bool write_file_bytes(const std::filesystem::path& p, const void* data, usize n) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(static_cast<const char*>(data), static_cast<std::streamsize>(n));
    return f.good();
}

bool write_file_text(const std::filesystem::path& p, std::string_view text) {
    return write_file_bytes(p, text.data(), text.size());
}

std::filesystem::path executable_dir() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0) return std::filesystem::current_path();
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path engine_root() {
    auto p = executable_dir();
    for (int i = 0; i < 8; ++i) {
        if (std::filesystem::exists(p / "vcpkg.json")) return p;
        if (!p.has_parent_path()) break;
        p = p.parent_path();
    }
    return executable_dir();
}

// ----------------------------------------------------------------------------
// Directory watcher (Win32 ReadDirectoryChangesW)
// ----------------------------------------------------------------------------
DirectoryWatcher::~DirectoryWatcher() { stop(); }

bool DirectoryWatcher::start(const std::filesystem::path& root, Callback cb) {
    if (running_) stop();
    root_ = root;
    cb_   = std::move(cb);
#if defined(_WIN32)
    HANDLE h = CreateFileW(
        root.wstring().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        CN_WARN("io", "DirectoryWatcher: CreateFileW failed for {} (err={})",
                root.string(), GetLastError());
        return false;
    }
    dir_handle_ = h;
    running_    = true;
    thread_ = std::thread([this] {
        constexpr DWORD kBufSize = 64 * 1024;
        std::vector<u8> buf(kBufSize);
        while (running_) {
            DWORD bytes = 0;
            BOOL ok = ReadDirectoryChangesW(
                static_cast<HANDLE>(dir_handle_),
                buf.data(), kBufSize, TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME  |
                FILE_NOTIFY_CHANGE_LAST_WRITE|
                FILE_NOTIFY_CHANGE_SIZE      |
                FILE_NOTIFY_CHANGE_CREATION,
                &bytes, nullptr, nullptr);
            if (!ok || !running_) break;
            const u8* p = buf.data();
            while (true) {
                auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(p);
                std::wstring name(info->FileName, info->FileNameLength / sizeof(WCHAR));
                ChangeEvent ev;
                ev.path = root_ / name;
                switch (info->Action) {
                    case FILE_ACTION_ADDED:    ev.kind = ChangeEvent::Kind::Created;  break;
                    case FILE_ACTION_REMOVED:  ev.kind = ChangeEvent::Kind::Deleted;  break;
                    case FILE_ACTION_MODIFIED: ev.kind = ChangeEvent::Kind::Modified; break;
                    case FILE_ACTION_RENAMED_OLD_NAME:
                    case FILE_ACTION_RENAMED_NEW_NAME: ev.kind = ChangeEvent::Kind::Renamed; break;
                    default: ev.kind = ChangeEvent::Kind::Modified;
                }
                if (cb_) cb_(ev);
                if (info->NextEntryOffset == 0) break;
                p += info->NextEntryOffset;
            }
        }
    });
    return true;
#else
    return false;
#endif
}

void DirectoryWatcher::stop() {
    if (!running_) return;
    running_ = false;
#if defined(_WIN32)
    if (dir_handle_) {
        // Cancel the blocking ReadDirectoryChangesW call.
        CancelIoEx(static_cast<HANDLE>(dir_handle_), nullptr);
        CloseHandle(static_cast<HANDLE>(dir_handle_));
        dir_handle_ = nullptr;
    }
#endif
    if (thread_.joinable()) thread_.join();
}

} // namespace cn::io
