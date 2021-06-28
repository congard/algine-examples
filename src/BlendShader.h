#ifndef BLENDSHADER_H
#define BLENDSHADER_H

#define constant(name, val) constexpr char name[] = val;

namespace BlendShader::Vars {
constant(BaseImage, "image")
constant(BloomImage, "bloom")
constant(DofImage, "dof")
constant(COCMap, "cocMap")
constant(DofSigmaDivider, "dofSigmaDivider")
constant(Exposure, "exposure")
constant(Gamma, "gamma")
}

#undef constant

#endif //BLENDSHADER_H
