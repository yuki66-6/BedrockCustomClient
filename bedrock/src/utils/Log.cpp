#include "utils/Log.h"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>
#include <stdexcept>

namespace bcc::log {
namespace {

std::mutex g_mutex;
FILE* g_file = nullptr;
std::atomic<bool> g_throwOnAssert{false};

const char* levelName(Level level) {
    switch (level) {
    case Level::Debug: return "DEBUG";
    case Level::Info: return "INFO ";
    case Level::Warn: return "WARN ";
    case Level::Error: return "ERROR";
    }
    return "?";
}

} // namespace

void init(const std::filesystem::path& file) {
    std::lock_guard lock(g_mutex);
    if (g_file) return;
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (std::filesystem::exists(file, ec) && std::filesystem::file_size(file, ec) > 1024 * 1024) {
        auto old = file;
        old += L".old";
        std::filesystem::rename(file, old, ec);
    }
    g_file = _wfsopen(file.c_str(), L"ab", _SH_DENYWR);
}

void shutdown() {
    std::lock_guard lock(g_mutex);
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
}

void write(Level level, std::string_view message) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::string line = std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} [{}] {}\n", t.wYear, t.wMonth, t.wDay,
                                   t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, levelName(level), message);
    std::lock_guard lock(g_mutex);
    OutputDebugStringA(line.c_str());
    if (g_file) {
        fwrite(line.data(), 1, line.size(), g_file);
        fflush(g_file);
    }
}

void setThrowOnImGuiAssert(bool enabled) { g_throwOnAssert = enabled; }

} // namespace bcc::log

void BccImGuiAssert(const char* expr, const char* file, int line) {
    if (bcc::log::g_throwOnAssert) {
        throw std::runtime_error(std::format("IM_ASSERT({}) {}:{}", expr, file, line));
    }
    // Une même assertion ne doit pas inonder le journal à chaque image.
    static std::mutex m;
    static std::set<std::pair<const char*, int>> seen;
    {
        std::lock_guard lock(m);
        if (!seen.insert({file, line}).second) return;
    }
    bcc::log::error("Assertion ImGui : {} ({}:{})", expr, file, line);
}
