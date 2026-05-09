// Hot reload of a gameplay DLL.
//
// Workflow:
//   1. Engine watches a target DLL path. When the file changes (mtime newer
//      than what we recorded), it copies the file to a side-by-side shadow
//      copy ("gameplay_loaded_<n>.dll") so the original can be re-linked while
//      the previous instance is still loaded.
//   2. We snapshot the world's ScriptComponent instance state via reflection
//      into a transient JSON buffer keyed by Entity id + class name.
//   3. We unload the old DLL (after destroying old instances).
//   4. We LoadLibrary the new shadow copy and re-call the registry export.
//   5. We rebuild the instances by class name and restore state from the JSON
//      snapshot.
//
// The gameplay DLL must export a function called "cn_gameplay_register" that
// the engine calls with a GameplayRegistry* it fills in.

#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/ecs/Entity.h"
#include "continuous/reflect/Reflect.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace cn::scene { class Scene; }
namespace cn::platform { class Window; class Input; }

namespace cn::gameplay {

// Common context passed to behaviors each tick.
struct CN_API Context {
    f32 dt = 0.0f;
    f32 elapsed = 0.0f;
    cn::scene::Scene*    scene = nullptr;
    cn::platform::Window* window = nullptr;
};

// Base class for user gameplay components. The user subclasses this in the
// gameplay DLL, registers the subclass via CN_GAMEPLAY_REGISTER, and the
// engine instantiates / ticks it.
class CN_API Behavior {
public:
    virtual ~Behavior() = default;
    virtual void on_start (Context& /*ctx*/) {}
    virtual void on_update(Context& /*ctx*/) {}
    virtual void on_stop  (Context& /*ctx*/) {}

    // Reflection hook so the inspector can show the behavior's fields.
    virtual const cn::reflect::TypeInfo* type_info() const { return nullptr; }

    cn::ecs::Entity owner;
};

using BehaviorFactory = std::function<std::unique_ptr<Behavior>()>;

class CN_API Registry {
public:
    static Registry& get();

    void register_class(const std::string& name, BehaviorFactory factory,
                        const cn::reflect::TypeInfo* ti = nullptr);
    std::unique_ptr<Behavior> create(const std::string& name) const;
    const cn::reflect::TypeInfo* type_for(const std::string& name) const;
    std::vector<std::string> class_names() const;
    void clear();

private:
    Registry() = default;
    struct Entry {
        BehaviorFactory factory;
        const cn::reflect::TypeInfo* type = nullptr;
    };
    std::unordered_map<std::string, Entry> entries_;
};

// Macro the gameplay author writes (in the gameplay DLL) to make the engine
// aware of a Behavior subclass.
#define CN_GAMEPLAY_REGISTER(ClassName)                                                  \
    namespace { struct AutoRegister_##ClassName {                                         \
        AutoRegister_##ClassName() {                                                      \
            cn::gameplay::Registry::get().register_class(                                 \
                #ClassName,                                                                \
                [] { return std::unique_ptr<cn::gameplay::Behavior>(new ClassName()); },  \
                cn::reflect::type_of<ClassName>());                                       \
        }                                                                                 \
    } CN_UNIQUE(_auto_register_); }

// The gameplay DLL must export this symbol so the engine can rebuild the
// registry every reload (the DLL's TU-level constructors do the work, but
// some compilers strip unused C++ initializers across DLL boundaries; calling
// this function references the registry.cpp TU and pulls everything in).
extern "C" CN_GAMEPLAY_API void cn_gameplay_register();

// ---------------------------------------------------------------------------
// Hot reload manager. Used by the editor and by the runtime when --hot-reload
// is passed. The runtime can also use it for live-tweak builds.
// ---------------------------------------------------------------------------
class CN_API HotReloader {
public:
    HotReloader() = default;
    ~HotReloader();
    CN_NONCOPYABLE(HotReloader);

    bool init(const std::filesystem::path& target_dll);
    void shutdown();

    // Returns true if a reload happened this frame.
    bool poll(cn::scene::Scene& scene);

    bool loaded() const { return module_ != nullptr; }
    const std::filesystem::path& target() const { return target_; }

private:
    bool load_module_();
    void unload_module_();

    std::filesystem::path target_;
    std::filesystem::path shadow_;
    void*                 module_ = nullptr;
    std::filesystem::file_time_type last_mtime_{};
    u32                   reload_counter_ = 0;
};

// Helpers used by editor + runtime to attach Behavior instances to entities
// via ScriptComponent.
CN_API void instantiate_behaviors(cn::scene::Scene& scene);
CN_API void update_behaviors    (cn::scene::Scene& scene, gameplay::Context& ctx);
CN_API void destroy_behaviors   (cn::scene::Scene& scene);
CN_API std::string snapshot_behavior_state(cn::scene::Scene& scene);
CN_API void        restore_behavior_state (cn::scene::Scene& scene, const std::string& snapshot);

} // namespace cn::gameplay
