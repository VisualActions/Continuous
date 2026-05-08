#include "continuous/reflect/Reflect.h"

namespace cn::reflect {

Registry& Registry::get() {
    static Registry r;
    return r;
}

TypeInfo& Registry::register_type(const char* name, usize size, usize align) {
    auto it = by_name_.find(name);
    if (it != by_name_.end()) return *it->second;
    auto p = std::make_unique<TypeInfo>();
    p->name = name;
    p->size = size;
    p->alignment = align;
    TypeInfo* raw = p.get();
    storage_.push_back(std::move(p));
    by_name_.emplace(raw->name, raw);
    ordered_.push_back(raw);
    return *raw;
}

void Registry::finish_type(const TypeInfo& /*info*/) {
    // Currently a no-op; here to give us a hook later (validation, hashing).
}

const TypeInfo* Registry::find(const std::string& name) const {
    auto it = by_name_.find(name);
    return it == by_name_.end() ? nullptr : it->second;
}

} // namespace cn::reflect
