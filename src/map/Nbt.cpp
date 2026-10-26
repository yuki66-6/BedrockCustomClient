#include "map/Nbt.h"

#include <cstring>

namespace bcc::nbt {
namespace {

template <class T>
bool readRaw(std::string_view& in, T& v) {
    if (in.size() < sizeof(T)) return false;
    std::memcpy(&v, in.data(), sizeof(T));
    in.remove_prefix(sizeof(T));
    return true;
}

bool readString(std::string_view& in, std::string& s) {
    uint16_t len;
    if (!readRaw(in, len) || in.size() < len) return false;
    s.assign(in.data(), len);
    in.remove_prefix(len);
    return true;
}

bool readPayload(std::string_view& in, Tag type, Value& out, int depth) {
    if (depth > 64) return false; // protection contre les données malformées
    out.type = type;
    switch (type) {
    case Byte: {
        int8_t v;
        if (!readRaw(in, v)) return false;
        out.i = v;
        return true;
    }
    case Short: {
        int16_t v;
        if (!readRaw(in, v)) return false;
        out.i = v;
        return true;
    }
    case Int: {
        int32_t v;
        if (!readRaw(in, v)) return false;
        out.i = v;
        return true;
    }
    case Long: return readRaw(in, out.i);
    case Float: {
        float v;
        if (!readRaw(in, v)) return false;
        out.d = v;
        return true;
    }
    case Double: return readRaw(in, out.d);
    case String: return readString(in, out.s);
    case ByteArray:
    case IntArray:
    case LongArray: {
        int32_t n;
        if (!readRaw(in, n) || n < 0) return false;
        size_t elem = type == ByteArray ? 1 : (type == IntArray ? 4 : 8);
        if (in.size() < static_cast<size_t>(n) * elem) return false;
        in.remove_prefix(static_cast<size_t>(n) * elem); // contenu non utilisé
        return true;
    }
    case List: {
        uint8_t elemType;
        int32_t n;
        if (!readRaw(in, elemType) || !readRaw(in, n) || n < 0 || elemType > LongArray) return false;
        if (n > 1 << 20) return false;
        out.list.resize(static_cast<size_t>(n));
        for (auto& v : out.list)
            if (!readPayload(in, static_cast<Tag>(elemType), v, depth + 1)) return false;
        return true;
    }
    case Compound: {
        while (true) {
            std::string name;
            Value v;
            if (in.empty()) return false;
            if (static_cast<uint8_t>(in[0]) == End) {
                in.remove_prefix(1);
                return true;
            }
            if (!readNamed(in, name, v, depth + 1)) return false;
            out.items.emplace_back(std::move(name), std::move(v));
        }
    }
    default: return false;
    }
}

} // namespace

bool readNamed(std::string_view& in, std::string& name, Value& out, int depth) {
    uint8_t type;
    if (!readRaw(in, type) || type == End || type > LongArray) return false;
    if (!readString(in, name)) return false;
    return readPayload(in, static_cast<Tag>(type), out, depth);
}

const Value* Value::find(std::string_view name) const {
    for (auto& [k, v] : items)
        if (k == name) return &v;
    return nullptr;
}

int64_t Value::getInt(std::string_view name, int64_t def) const {
    const Value* v = find(name);
    return v && v->type >= Byte && v->type <= Long ? v->i : def;
}

double Value::getNumber(std::string_view name, double def) const {
    const Value* v = find(name);
    if (!v) return def;
    if (v->type == Float || v->type == Double) return v->d;
    if (v->type >= Byte && v->type <= Long) return static_cast<double>(v->i);
    return def;
}

std::string Value::getString(std::string_view name, std::string def) const {
    const Value* v = find(name);
    return v && v->type == String ? v->s : def;
}

} // namespace bcc::nbt
