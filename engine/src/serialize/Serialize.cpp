#include "continuous/serialize/Serialize.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"

#include <cstring>

namespace cn::serialize {

using namespace cn::reflect;

// ----------------------------------------------------------------------------
// helpers: read/write field as JSON value
// ----------------------------------------------------------------------------
static json field_to_json(const FieldInfo& f, const void* obj) {
    const u8* base = static_cast<const u8*>(obj) + f.offset;
    switch (f.type) {
        case FieldType::Bool:   return *reinterpret_cast<const bool*>(base);
        case FieldType::I32:    return *reinterpret_cast<const i32*>(base);
        case FieldType::U32:    return *reinterpret_cast<const u32*>(base);
        case FieldType::F32:    return *reinterpret_cast<const f32*>(base);
        case FieldType::F64:    return *reinterpret_cast<const f64*>(base);
        case FieldType::String: return *reinterpret_cast<const std::string*>(base);
        case FieldType::Vec2: {
            auto* v = reinterpret_cast<const math::vec2*>(base);
            return json{ v->x, v->y };
        }
        case FieldType::Vec3: {
            auto* v = reinterpret_cast<const math::vec3*>(base);
            return json{ v->x, v->y, v->z };
        }
        case FieldType::Vec4:
        case FieldType::Color: {
            auto* v = reinterpret_cast<const math::vec4*>(base);
            return json{ v->x, v->y, v->z, v->w };
        }
        case FieldType::Quat: {
            auto* v = reinterpret_cast<const math::quat*>(base);
            return json{ v->w, v->x, v->y, v->z };
        }
        case FieldType::Enum: {
            i64 raw = 0;
            std::memcpy(&raw, base, std::min<usize>(f.size, sizeof(i64)));
            for (auto& ev : f.enum_values) if (ev.value == raw) return ev.name;
            return raw;
        }
        case FieldType::Struct: {
            json sub;
            if (f.inner) to_json(sub, *f.inner, base);
            return sub;
        }
        case FieldType::VectorOf: {
            json arr = json::array();
            usize n = f.vec_size(base);
            for (usize i = 0; i < n; ++i) {
                json elem;
                if (f.inner) to_json(elem, *f.inner, f.vec_at_const(base, i));
                arr.push_back(elem);
            }
            return arr;
        }
        case FieldType::AssetRef:
        case FieldType::EntityRef:
            return *reinterpret_cast<const u64*>(base);
        default: return nullptr;
    }
}

static void field_from_json(const FieldInfo& f, void* obj, const json& j) {
    u8* base = static_cast<u8*>(obj) + f.offset;
    switch (f.type) {
        case FieldType::Bool: if (j.is_boolean()) *reinterpret_cast<bool*>(base) = j.get<bool>(); break;
        case FieldType::I32:  if (j.is_number())  *reinterpret_cast<i32*>(base)  = j.get<i32>();  break;
        case FieldType::U32:  if (j.is_number())  *reinterpret_cast<u32*>(base)  = j.get<u32>();  break;
        case FieldType::F32:  if (j.is_number())  *reinterpret_cast<f32*>(base)  = j.get<f32>();  break;
        case FieldType::F64:  if (j.is_number())  *reinterpret_cast<f64*>(base)  = j.get<f64>();  break;
        case FieldType::String:
            if (j.is_string()) *reinterpret_cast<std::string*>(base) = j.get<std::string>(); break;
        case FieldType::Vec2:
            if (j.is_array() && j.size() >= 2) {
                auto* v = reinterpret_cast<math::vec2*>(base);
                v->x = j[0].get<f32>(); v->y = j[1].get<f32>();
            } break;
        case FieldType::Vec3:
            if (j.is_array() && j.size() >= 3) {
                auto* v = reinterpret_cast<math::vec3*>(base);
                v->x = j[0].get<f32>(); v->y = j[1].get<f32>(); v->z = j[2].get<f32>();
            } break;
        case FieldType::Vec4:
        case FieldType::Color:
            if (j.is_array() && j.size() >= 4) {
                auto* v = reinterpret_cast<math::vec4*>(base);
                v->x = j[0].get<f32>(); v->y = j[1].get<f32>();
                v->z = j[2].get<f32>(); v->w = j[3].get<f32>();
            } break;
        case FieldType::Quat:
            if (j.is_array() && j.size() >= 4) {
                auto* v = reinterpret_cast<math::quat*>(base);
                v->w = j[0].get<f32>(); v->x = j[1].get<f32>();
                v->y = j[2].get<f32>(); v->z = j[3].get<f32>();
            } break;
        case FieldType::Enum: {
            i64 raw = 0;
            if (j.is_string()) {
                std::string name = j.get<std::string>();
                for (auto& ev : f.enum_values) if (ev.name == name) { raw = ev.value; break; }
            } else if (j.is_number()) {
                raw = j.get<i64>();
            }
            std::memcpy(base, &raw, std::min<usize>(f.size, sizeof(i64)));
        } break;
        case FieldType::Struct:
            if (f.inner) from_json(j, *f.inner, base);
            break;
        case FieldType::VectorOf: {
            if (j.is_array() && f.inner) {
                f.vec_resize(base, 0);
                f.vec_resize(base, j.size());
                for (usize i = 0; i < j.size(); ++i) {
                    from_json(j[i], *f.inner, f.vec_at(base, i));
                }
            }
        } break;
        case FieldType::AssetRef:
        case FieldType::EntityRef:
            if (j.is_number()) *reinterpret_cast<u64*>(base) = j.get<u64>(); break;
        default: break;
    }
}

void to_json(json& j, const TypeInfo& ti, const void* obj) {
    j = json::object();
    j["__type"] = ti.name;
    for (auto& f : ti.fields) {
        j[f.name] = field_to_json(f, obj);
    }
}

void from_json(const json& j, const TypeInfo& ti, void* obj) {
    if (!j.is_object()) return;
    for (auto& f : ti.fields) {
        auto it = j.find(f.name);
        if (it != j.end()) field_from_json(f, obj, *it);
    }
}

// ----------------------------------------------------------------------------
// Binary
// ----------------------------------------------------------------------------
void BinaryWriter::write_raw(const void* data, usize bytes) {
    auto* p = static_cast<const u8*>(data);
    bytes_.insert(bytes_.end(), p, p + bytes);
}

void BinaryWriter::write_string(std::string_view s) {
    u32 n = static_cast<u32>(s.size());
    write_pod(n);
    if (n) write_raw(s.data(), n);
}

bool BinaryReader::read_raw(void* dst, usize bytes) {
    if (cursor_ + bytes > bytes_.size()) return false;
    std::memcpy(dst, bytes_.data() + cursor_, bytes);
    cursor_ += bytes;
    return true;
}

bool BinaryReader::read_string(std::string& out) {
    u32 n = 0;
    if (!read_pod(n)) return false;
    out.assign(n, '\0');
    if (n && !read_raw(out.data(), n)) return false;
    return true;
}

static void write_field_binary(BinaryWriter& w, const FieldInfo& f, const void* obj) {
    const u8* base = static_cast<const u8*>(obj) + f.offset;
    switch (f.type) {
        case FieldType::Bool:
        case FieldType::I32:
        case FieldType::U32:
        case FieldType::F32:
        case FieldType::F64:
        case FieldType::Vec2:
        case FieldType::Vec3:
        case FieldType::Vec4:
        case FieldType::Color:
        case FieldType::Quat:
        case FieldType::Enum:
        case FieldType::AssetRef:
        case FieldType::EntityRef:
            w.write_raw(base, f.size);
            break;
        case FieldType::String:
            w.write_string(*reinterpret_cast<const std::string*>(base));
            break;
        case FieldType::Struct:
            if (f.inner) write_binary(w, *f.inner, base);
            break;
        case FieldType::VectorOf: {
            u32 n = static_cast<u32>(f.vec_size(base));
            w.write_pod(n);
            if (f.inner) {
                for (u32 i = 0; i < n; ++i) write_binary(w, *f.inner, f.vec_at_const(base, i));
            }
        } break;
        default: break;
    }
}

static bool read_field_binary(BinaryReader& r, const FieldInfo& f, void* obj) {
    u8* base = static_cast<u8*>(obj) + f.offset;
    switch (f.type) {
        case FieldType::Bool:
        case FieldType::I32:
        case FieldType::U32:
        case FieldType::F32:
        case FieldType::F64:
        case FieldType::Vec2:
        case FieldType::Vec3:
        case FieldType::Vec4:
        case FieldType::Color:
        case FieldType::Quat:
        case FieldType::Enum:
        case FieldType::AssetRef:
        case FieldType::EntityRef:
            return r.read_raw(base, f.size);
        case FieldType::String:
            return r.read_string(*reinterpret_cast<std::string*>(base));
        case FieldType::Struct:
            return f.inner ? read_binary(r, *f.inner, base) : true;
        case FieldType::VectorOf: {
            u32 n = 0;
            if (!r.read_pod(n)) return false;
            f.vec_resize(base, 0);
            f.vec_resize(base, n);
            if (f.inner) {
                for (u32 i = 0; i < n; ++i) {
                    if (!read_binary(r, *f.inner, f.vec_at(base, i))) return false;
                }
            }
            return true;
        }
        default: return true;
    }
}

void write_binary(BinaryWriter& w, const TypeInfo& ti, const void* obj) {
    // Tag-by-name keeps us self-describing enough to fail loudly on mismatch.
    w.write_string(ti.name);
    u32 nfields = static_cast<u32>(ti.fields.size());
    w.write_pod(nfields);
    for (auto& f : ti.fields) {
        w.write_string(f.name);
        u32 ty = static_cast<u32>(f.type);
        w.write_pod(ty);
        write_field_binary(w, f, obj);
    }
}

bool read_binary(BinaryReader& r, const TypeInfo& ti, void* obj) {
    std::string name;
    if (!r.read_string(name)) return false;
    if (name != ti.name) {
        CN_WARN("serialize", "binary tag '{}' does not match expected '{}'", name, ti.name);
        return false;
    }
    u32 nfields = 0;
    if (!r.read_pod(nfields)) return false;
    for (u32 i = 0; i < nfields; ++i) {
        std::string fname;
        u32 ty = 0;
        if (!r.read_string(fname)) return false;
        if (!r.read_pod(ty)) return false;

        // Locate field in our schema.
        const FieldInfo* match = nullptr;
        for (auto& f : ti.fields) if (f.name == fname) { match = &f; break; }
        if (match && static_cast<u32>(match->type) == ty) {
            if (!read_field_binary(r, *match, obj)) return false;
        } else {
            // Schema drift: skip unknown field by re-reading into a dummy of the
            // recorded type. We don't have generic skip without knowing sizes,
            // so log and bail - cooked binaries are regenerated on schema change.
            CN_WARN("serialize", "schema drift on '{}::{}'", ti.name, fname);
            return false;
        }
    }
    return true;
}

} // namespace cn::serialize
