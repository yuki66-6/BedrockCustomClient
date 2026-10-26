#pragma once

#include <string>
#include <string_view>

namespace bcc {

std::wstring toWide(std::string_view utf8);
std::string toUtf8(std::wstring_view wide);

// Message lisible pour un code d'erreur Win32 / HRESULT.
std::string win32ErrorMessage(unsigned long code);

} // namespace bcc
