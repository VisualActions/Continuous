// Continuous Engine - reflection system.
//
// Approach: macro-based registry, no preprocessor magic, no external code
// generation step. Each reflectable type provides a static reflect_type()
// method (via the CN_REFLECT_BEGIN/CN_REFLECT_FIELD/CN_REFLECT_END macros) that
// describes the type to a Registry. The registry stores TypeInfo records.
//
// Field type erasure: each field has get/set callbacks taking void* (the
// owning object) and a TypeId discriminator that the inspector / serializer
// switches on. This is enough for the inspector to draw widgets and for the
// JSON serializer to serialize/deserialize, without templating the entire
// system on every reflectable type.
//
// What the inspector understands directly:
//   - bool, i32, u32, f32, f64, std::string
//   - vec2, vec3, vec4, quat, color (vec4)
//   - enum class (registered via CN_REFLECT_ENUM)
//   - nested struct (drawn recursively)
//   - std::vector<T> for any registered T (resizable list)
//
// Adding new field types: extend FieldType and the inspector / serializer
// switches. The registry layer itself is type-agnostic.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/String.h"
#include "continuous/core/Types.h"
#include "continuous/math/Math.h"

#include <functional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace cn::reflect {

enum class FieldType : u32 {
    None,
    Bool,
    I32,
    U32,
    F32,
    F64,
    String,
    Vec2,
    Vec3,
    Vec4,
    Quat,
    Color,
    Enum,
    Struct,
    VectorOf,
    AssetRef,
    EntityRef,
};

struct TypeInfo;

struct EnumValue {
    std::string name;
    i64         value;
};

struct FieldInfo {
    std::string name;
    FieldType   type{FieldType::None};
    usize       offset{0};
    usize       size{0};

    // For nested struct / vector element / enum, this points at the inner
    // type's TypeInfo.
    const TypeInfo* inner{nullptr};

    // Enum values, populated only when type == Enum.
    std::vector<EnumValue> enum_values;

    // Vector accessors (populated only when type == VectorOf).
    std::function<usize(const void*)>          vec_size;
    std::function<void(void*, usize)>          vec_resize;
    std::function<void*(void*, usize)>         vec_at;
    std::function<const void*(const void*, usize)> vec_at_const;
    std::function<void(void*)>                 vec_push_default;
    std::function<void(void*, usize)>          vec_erase;

    // Editor metadata.
    f32  drag_min{ 0.0f };
    f32  drag_max{ 0.0f };
    f32  drag_speed{ 0.1f };
    bool is_color{ false };
    std::string tooltip;
};

struct TypeInfo {
    std::string name;
    usize       size{0};
    usize       alignment{0};
    std::vector<FieldInfo> fields;

    // Polymorphic ops.
    std::function<void*()>             create;
    std::function<void(void*)>         destroy;
    std::function<void(void*, const void*)> copy;
};

class CN_API Registry {
public:
    static Registry& get();

    TypeInfo& register_type(const char* name, usize size, usize align);
    void      finish_type(const TypeInfo& info);

    const TypeInfo* find(const std::string& name) const;
    const std::vector<TypeInfo*>& types() const { return ordered_; }

private:
    Registry() = default;
    std::unordered_map<std::string, TypeInfo*> by_name_;
    std::vector<std::unique_ptr<TypeInfo>>     storage_;
    std::vector<TypeInfo*>                     ordered_;
};

// Per-type accessor: every reflectable type specializes Reflector<T>::get().
template <typename T>
struct Reflector {
    static const TypeInfo* get() { return nullptr; }
};

template <typename T>
inline const TypeInfo* type_of() { return Reflector<T>::get(); }

} // namespace cn::reflect

// ============================================================================
// Macros
// ============================================================================

#define CN_REFLECT_DECL(T)                                              \
    namespace cn::reflect {                                             \
    template <> struct Reflector<T> {                                   \
        static const TypeInfo* get();                                   \
    };                                                                  \
    } // namespace cn::reflect

#define CN_REFLECT_BEGIN(T)                                             \
    const ::cn::reflect::TypeInfo* ::cn::reflect::Reflector<T>::get() { \
        using Self = T;                                                 \
        static const ::cn::reflect::TypeInfo* s_info = nullptr;         \
        if (s_info) return s_info;                                      \
        auto& reg = ::cn::reflect::Registry::get();                     \
        auto& info = reg.register_type(#T, sizeof(T), alignof(T));      \
        info.create  = [] { return static_cast<void*>(new T()); };      \
        info.destroy = [](void* p) { delete static_cast<T*>(p); };      \
        info.copy    = [](void* dst, const void* src) {                 \
            *static_cast<T*>(dst) = *static_cast<const T*>(src);         \
        };

#define CN_REFLECT_FIELD(member, type_enum)                                              \
        {                                                                                \
            ::cn::reflect::FieldInfo f;                                                  \
            f.name   = #member;                                                          \
            f.type   = ::cn::reflect::FieldType::type_enum;                              \
            f.offset = offsetof(Self, member);                                           \
            f.size   = sizeof(((Self*)0)->member);                                       \
            info.fields.push_back(f);                                                    \
        }

#define CN_REFLECT_FIELD_RANGE(member, type_enum, lo, hi, speed)                         \
        {                                                                                \
            ::cn::reflect::FieldInfo f;                                                  \
            f.name      = #member;                                                       \
            f.type      = ::cn::reflect::FieldType::type_enum;                           \
            f.offset    = offsetof(Self, member);                                        \
            f.size      = sizeof(((Self*)0)->member);                                    \
            f.drag_min  = (lo);                                                          \
            f.drag_max  = (hi);                                                          \
            f.drag_speed= (speed);                                                       \
            info.fields.push_back(f);                                                    \
        }

#define CN_REFLECT_FIELD_COLOR(member)                                                   \
        {                                                                                \
            ::cn::reflect::FieldInfo f;                                                  \
            f.name      = #member;                                                       \
            f.type      = ::cn::reflect::FieldType::Color;                               \
            f.offset    = offsetof(Self, member);                                        \
            f.size      = sizeof(((Self*)0)->member);                                    \
            f.is_color  = true;                                                          \
            info.fields.push_back(f);                                                    \
        }

#define CN_REFLECT_FIELD_STRUCT(member, InnerT)                                          \
        {                                                                                \
            ::cn::reflect::FieldInfo f;                                                  \
            f.name   = #member;                                                          \
            f.type   = ::cn::reflect::FieldType::Struct;                                 \
            f.offset = offsetof(Self, member);                                           \
            f.size   = sizeof(((Self*)0)->member);                                       \
            f.inner  = ::cn::reflect::type_of<InnerT>();                                 \
            info.fields.push_back(f);                                                    \
        }

#define CN_REFLECT_FIELD_VEC(member, InnerT)                                             \
        {                                                                                \
            using VecT = decltype(((Self*)0)->member);                                   \
            ::cn::reflect::FieldInfo f;                                                  \
            f.name   = #member;                                                          \
            f.type   = ::cn::reflect::FieldType::VectorOf;                               \
            f.offset = offsetof(Self, member);                                           \
            f.size   = sizeof(((Self*)0)->member);                                       \
            f.inner  = ::cn::reflect::type_of<InnerT>();                                 \
            f.vec_size       = [](const void* v){ return static_cast<const VecT*>(v)->size(); };\
            f.vec_resize     = [](void* v, usize n){ static_cast<VecT*>(v)->resize(n); };\
            f.vec_at         = [](void* v, usize i){ return static_cast<void*>(&(*static_cast<VecT*>(v))[i]); };\
            f.vec_at_const   = [](const void* v, usize i){ return static_cast<const void*>(&(*static_cast<const VecT*>(v))[i]); };\
            f.vec_push_default = [](void* v){ static_cast<VecT*>(v)->emplace_back(); };  \
            f.vec_erase      = [](void* v, usize i){ auto* vv = static_cast<VecT*>(v); vv->erase(vv->begin()+i); };\
            info.fields.push_back(f);                                                    \
        }

#define CN_REFLECT_END(T)                                                                \
        reg.finish_type(info);                                                           \
        s_info = &info;                                                                  \
        return s_info;                                                                   \
    }

// Enum reflection.
#define CN_REFLECT_ENUM_BEGIN(E)                                                         \
    const ::cn::reflect::TypeInfo* ::cn::reflect::Reflector<E>::get() {                  \
        using Self = E;                                                                  \
        static const ::cn::reflect::TypeInfo* s_info = nullptr;                          \
        if (s_info) return s_info;                                                       \
        auto& reg = ::cn::reflect::Registry::get();                                      \
        auto& info = reg.register_type(#E, sizeof(E), alignof(E));                       \
        ::cn::reflect::FieldInfo f;                                                      \
        f.name = "value";                                                                \
        f.type = ::cn::reflect::FieldType::Enum;                                         \
        f.offset = 0;                                                                    \
        f.size = sizeof(E);

#define CN_REFLECT_ENUM_VALUE(name)                                                      \
        f.enum_values.push_back({ #name, static_cast<::cn::i64>(Self::name) });

#define CN_REFLECT_ENUM_END(E)                                                           \
        info.fields.push_back(f);                                                        \
        reg.finish_type(info);                                                           \
        s_info = &info;                                                                  \
        return s_info;                                                                   \
    }
