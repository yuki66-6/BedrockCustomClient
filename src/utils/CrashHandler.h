#pragma once

#include <filesystem>

namespace bcc::crash {

// Journalise les exceptions fatales (code, fonction, fichier:ligne si les
// symboles sont disponibles) et écrit un minidump dans `dumpDir`.
void install(const std::filesystem::path& dumpDir);

} // namespace bcc::crash
