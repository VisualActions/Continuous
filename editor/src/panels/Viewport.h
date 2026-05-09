#pragma once

#include "continuous/Engine.h"

namespace cnedit {

class ViewportPanel {
public:
    enum class Op { Translate, Rotate, Scale };
    enum class Space { World, Local };

    void draw(cn::Engine& eng, cn::ecs::Entity selected);

    Op    op    = Op::Translate;
    Space space = Space::World;

    cn::math::vec3 cam_pos {0, 3, -8};
    cn::math::vec3 cam_target {0, 1, 0};
    float          cam_yaw    = 0.0f;
    float          cam_pitch  = -10.0f;
    float          cam_dist   = 8.0f;
    float          fov_deg    = 60.0f;
    bool           dragging   = false;
    bool           wireframe  = false;
};

} // namespace cnedit
