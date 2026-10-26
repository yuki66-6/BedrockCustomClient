#include "utils/Strings.h"

#include <windows.h>

#include <format>

namespace bcc {

std::wstring toWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

std::string toUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::string win32ErrorMessage(unsigned long code) {
    wchar_t* buffer = nullptr;
    DWORD len = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                               nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::string msg;
    if (len && buffer) {
        msg = toUtf8(std::wstring_view(buffer, len));
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r' || msg.back() == ' ')) msg.pop_back();
    }
    if (buffer) LocalFree(buffer);
    return std::format("0x{:08X} {}", code, msg);
}

} // namespace bcc
