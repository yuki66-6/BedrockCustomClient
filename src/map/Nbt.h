#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// NBT petit-boutiste (format disque de Minecraft Bedrock), lecture seule.
namespace bcc::nbt {

enum Tag : uint8_t {
    End = 0, Byte, Short, Int, Long, Float, Double, ByteArray, String, List, Compound, IntArray, LongArray
};

struct Value {
    Tag type = End;
    int64_t i = 0;     // Byte/Short/Int/Long
    double d = 0;      // Float/Double
    std::string s;     // String
    std::vector<Value> list;                          // List (et tableaux convertis)
    std::vector<std::pair<std::string, Value>> items; // Compound

    const Value* find(std::string_view name) const;
    int64_t getInt(std::string_view name, int64_t def = 0) const;
    double getNumber(std::string_view name, double def = 0) const;
    std::string getString(std::string_view name, std::string def = {}) const;
};

// Lit un tag nommé (type + nom + contenu) au début de `in` et avance `in`.
bool readNamed(std::string_view& in, std::string& name, Value& out, int depth = 0);

} // namespace bcc::nbt
