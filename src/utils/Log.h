#pragma once

#include <filesystem>
#include <format>
#include <string>
#include <string_view>

namespace bcc::log {

enum class Level { Debug, Info, Warn, Error };

// Ouvre le fichier de log (rotation simple à 1 Mo). Sans appel à init(), les
// messages partent uniquement vers OutputDebugString.
void init(const std::filesystem::path& file);
void shutdown();
void write(Level level, std::string_view message);

template <class... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) { write(Level::Debug, std::format(fmt, std::forward<Args>(args)...)); }
template <class... Args>
void info(std::format_string<Args...> fmt, Args&&... args) { write(Level::Info, std::format(fmt, std::forward<Args>(args)...)); }
template <class... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) { write(Level::Warn, std::format(fmt, std::forward<Args>(args)...)); }
template <class... Args>
void error(std::format_string<Args...> fmt, Args&&... args) { write(Level::Error, std::format(fmt, std::forward<Args>(args)...)); }

// Les assertions ImGui (IM_ASSERT) passent par ici. Dans les tests on lève une
// exception pour faire échouer le cas ; dans l'application on journalise.
void setThrowOnImGuiAssert(bool enabled);

} // namespace bcc::log
