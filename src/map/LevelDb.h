#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// Lecteur LevelDB minimal, en LECTURE SEULE, pour la variante utilisée par
// Minecraft Bedrock (blocs compressés zlib / deflate brut). Les fichiers sont
// ouverts en partage complet et copiés en mémoire : aucun verrou n'est pris
// et rien n'est jamais écrit dans le dossier du monde.
namespace bcc::ldb {

enum class EntryType : uint8_t { Deletion = 0, Value = 1 };

struct Entry {
    std::string_view key;   // clé utilisateur
    std::string_view value; // vide pour une suppression
    uint64_t sequence = 0;
    EntryType type = EntryType::Value;
};

using Visitor = std::function<void(const Entry&)>;

// Lecture d'un fichier entier sans bloquer le jeu (FILE_SHARE_*).
bool readFileShared(const std::filesystem::path& file, std::string& out);

// Décompression d'un bloc selon le code de compression LevelDB/Mojang
// (0 aucune, 2 zlib, 4 deflate brut). Retourne false si non pris en charge.
bool decompressBlock(uint8_t compression, std::string_view in, std::string& out);

// Parcourt toutes les entrées d'une table SSTable (.ldb / .sst).
bool scanTable(std::string_view data, const Visitor& visit, std::string* error = nullptr);
// Parcourt toutes les entrées d'un journal (.log).
bool scanLog(std::string_view data, const Visitor& visit, std::string* error = nullptr);

struct ScanStats {
    int files = 0;
    int failedFiles = 0;
    uint64_t entries = 0;
    uint64_t bytes = 0;
};

// Parcourt toutes les tables et journaux du dossier `db`. Les doublons (plusieurs
// versions d'une même clé) sont tous visités : à l'appelant de garder la plus
// grande séquence. `cancel` permet d'interrompre ; `progress` reçoit [0,1].
ScanStats scanDatabase(const std::filesystem::path& dbDir, const Visitor& visit, const std::atomic<bool>* cancel = nullptr,
                       const std::function<void(float)>& progress = {});

// Utilitaires exposés pour les tests.
bool getVarint32(std::string_view& in, uint32_t& v);
bool getVarint64(std::string_view& in, uint64_t& v);

} // namespace bcc::ldb
