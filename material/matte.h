//
// Created by Shiina Miyuki on 2019/2/3.
//

#ifndef MIYUKI_MATTE_H
#define MIYUKI_MATTE_H

#include "../core/material.h"
#include "../core/reflection.h"

namespace Miyuki {
    class MatteMaterial : public Material {
        LambertianReflection reflection;
    public:
//        MatteMaterial(const MaterialInfo &info);

        void computeScatteringFunctions(Interaction &interaction) override;
    };
}
#endif //MIYUKI_MATTE_H
