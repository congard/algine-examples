#ifndef BLENDSHADER_H
#define BLENDSHADER_H

#define constant(name, val) constexpr char name[] = val;

namespace BlendShader {
    namespace Vars {
        constant(BaseImage, "image")
        constant(BloomImage, "bloom")
        constant(DofImage, "dof")
        constant(Exposure, "exposure")
        constant(Gamma, "gamma")
    }
}

#undef constant

#endif //BLENDSHADER_H
