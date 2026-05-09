#pragma once

#include "continuous/ecs/Entity.h"
#include "continuous/scene/Scene.h"

namespace cnedit {

class HierarchyPanel {
public:
    void draw(cn::scene::Scene& scene, cn::ecs::Entity& selected);
};

} // namespace cnedit
