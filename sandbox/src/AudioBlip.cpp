#include "AudioBlip.h"
#include "continuous/Engine.h"
#include "continuous/audio/Audio.h"
#include "continuous/scene/Components.h"

CN_REFLECT_BEGIN(::sb::AudioBlip)
    CN_REFLECT_FIELD_RANGE(interval, F32, 0.05f, 10.0f, 0.05f)
    CN_REFLECT_FIELD_RANGE(volume,   F32, 0.0f,  1.5f,  0.01f)
CN_REFLECT_END(::sb::AudioBlip)

namespace sb {

void AudioBlip::on_start(cn::gameplay::Context& /*ctx*/) {
    timer = 0.0f;
}

void AudioBlip::on_update(cn::gameplay::Context& ctx) {
    if (!ctx.scene) return;
    timer += ctx.dt;
    if (timer < interval) return;
    timer = 0.0f;
    auto* t = ctx.scene->world().get<cn::scene::TransformComponent>(owner);
    if (!t) return;
    cn::math::vec3 pos = cn::math::vec3(t->world[3]);
    // Use the audio source's clip if there is one, otherwise a reserved id.
    auto* src = ctx.scene->world().get<cn::scene::AudioSourceComponent>(owner);
    std::string clip = src && !src->clip_id.empty() ? src->clip_id : std::string("blip.wav");
    cn::audio::global().play_clip_3d(clip, pos, cn::audio::Bus::SFX,
                                     1.0f, 50.0f, volume, 1.0f, false);
}

const cn::reflect::TypeInfo* AudioBlip::type_info() const {
    return cn::reflect::type_of<AudioBlip>();
}

} // namespace sb

CN_GAMEPLAY_REGISTER(sb::AudioBlip)
