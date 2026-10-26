#pragma once

#include <filesystem>

namespace bcc::paths {

std::filesystem::path exeDir();

// Dossier de données : à côté de l'exécutable (mode portable) s'il est
// accessible en écriture, sinon %APPDATA%\BedrockCustomClient.
std::filesystem::path dataDir();
void setDataDir(const std::filesystem::path& dir); // tests / --data-dir

inline std::filesystem::path configsDir() { return dataDir() / "configs"; }

// Données communes Java/Bedrock (schémas, définitions de modules, profils
// prédéfinis) : <exe>/shared, sinon le dossier shared/ du dépôt en développement.
std::filesystem::path sharedDir();
void setSharedDir(const std::filesystem::path& dir); // tests
inline std::filesystem::path logsDir() { return dataDir() / "logs"; }

} // namespace bcc::paths
