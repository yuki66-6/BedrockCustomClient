#include "utils/Paths.h"

#include <windows.h>
#include <shlobj.h>

#include <fstream>

namespace bcc::paths {
namespace {

std::filesystem::path g_dataDir;

bool isWritable(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto probe = dir / L".bcc_write_test";
    {
        std::ofstream f(probe, std::ios::binary);
        if (!f) return false;
    }
    std::filesystem::remove(probe, ec);
    return true;
}

} // namespace

std::filesystem::path exeDir() {
    wchar_t buffer[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    return std::filesystem::path(std::wstring(buffer, n)).parent_path();
}

std::filesystem::path dataDir() {
    if (!g_dataDir.empty()) return g_dataDir;
    auto portable = exeDir();
    if (isWritable(portable)) {
        g_dataDir = portable;
    } else {
        PWSTR appData = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
            g_dataDir = std::filesystem::path(appData) / L"BedrockCustomClient";
            CoTaskMemFree(appData);
        } else {
            g_dataDir = portable;
        }
    }
    return g_dataDir;
}

void setDataDir(const std::filesystem::path& dir) { g_dataDir = dir; }

} // namespace bcc::paths
