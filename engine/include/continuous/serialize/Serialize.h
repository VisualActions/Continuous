// Continuous Engine - reflection-driven serialization (JSON + binary).
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/reflect/Reflect.h"

#include <nlohmann/json.hpp>
#include <span>
#include <vector>

namespace cn::serialize {

using json = nlohmann::json;

// ----------------------------------------------------------------------------
// JSON.
// ----------------------------------------------------------------------------
CN_API void to_json(json& j, const reflect::TypeInfo& ti, const void* obj);
CN_API void from_json(const json& j, const reflect::TypeInfo& ti, void* obj);

template <typename T>
inline json to_json(const T& obj) {
    json j;
    if (auto* ti = reflect::type_of<T>()) to_json(j, *ti, &obj);
    return j;
}

template <typename T>
inline void from_json(const json& j, T& obj) {
    if (auto* ti = reflect::type_of<T>()) from_json(j, *ti, &obj);
}

// ----------------------------------------------------------------------------
// Binary - little-endian, fixed format. Tagged with the type name so we can
// detect schema drift on load.
// ----------------------------------------------------------------------------
class CN_API BinaryWriter {
public:
    BinaryWriter() = default;
    void write_raw(const void* data, usize bytes);
    template <typename T> void write_pod(const T& v) { write_raw(&v, sizeof(T)); }
    void write_string(std::string_view s);
    const std::vector<u8>& bytes() const { return bytes_; }
    std::vector<u8> take() { return std::move(bytes_); }
private:
    std::vector<u8> bytes_;
};

class CN_API BinaryReader {
public:
    BinaryReader(std::span<const u8> bytes) : bytes_(bytes) {}
    bool read_raw(void* dst, usize bytes);
    template <typename T> bool read_pod(T& v) { return read_raw(&v, sizeof(T)); }
    bool read_string(std::string& out);
    bool eof() const { return cursor_ >= bytes_.size(); }
    usize position() const { return cursor_; }
private:
    std::span<const u8> bytes_;
    usize cursor_{0};
};

CN_API void write_binary(BinaryWriter& w, const reflect::TypeInfo& ti, const void* obj);
CN_API bool read_binary (BinaryReader& r, const reflect::TypeInfo& ti, void* obj);

template <typename T>
inline std::vector<u8> to_binary(const T& obj) {
    BinaryWriter w;
    if (auto* ti = reflect::type_of<T>()) write_binary(w, *ti, &obj);
    return w.take();
}

template <typename T>
inline bool from_binary(std::span<const u8> bytes, T& obj) {
    BinaryReader r(bytes);
    if (auto* ti = reflect::type_of<T>()) return read_binary(r, *ti, &obj);
    return false;
}

} // namespace cn::serialize
