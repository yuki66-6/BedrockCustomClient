#pragma once

#include <filesystem>
#include <optional>

namespace bcc::dialogs {

// Boîtes de dialogue Windows standard (comdlg32) filtrées sur les fichiers JSON.
std::optional<std::filesystem::path> openJson(void* owner);
std::optional<std::filesystem::path> saveJson(void* owner, const std::wstring& defaultName);

} // namespace bcc::dialogs
