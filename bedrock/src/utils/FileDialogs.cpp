#include "utils/FileDialogs.h"

#include <windows.h>
#include <commdlg.h>

#include <string>

namespace bcc::dialogs {
namespace {
constexpr wchar_t kFilter[] = L"Profil JSON (*.json)\0*.json\0Tous les fichiers (*.*)\0*.*\0";
}

std::optional<std::filesystem::path> openJson(void* owner) {
    wchar_t file[MAX_PATH * 2] = {};
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = static_cast<HWND>(owner);
    ofn.lpstrFilter = kFilter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.lpstrTitle = L"Importer un profil";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return std::nullopt;
    return std::filesystem::path(file);
}

std::optional<std::filesystem::path> saveJson(void* owner, const std::wstring& defaultName) {
    wchar_t file[MAX_PATH * 2] = {};
    wcsncpy_s(file, defaultName.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = static_cast<HWND>(owner);
    ofn.lpstrFilter = kFilter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.lpstrTitle = L"Exporter les réglages";
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) return std::nullopt;
    return std::filesystem::path(file);
}

} // namespace bcc::dialogs
