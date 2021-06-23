#ifndef COLORSHADER_H
#define COLORSHADER_H

#define constant(name, val) constexpr char name[] = val;

namespace ColorShader {
    namespace Settings {
        constant(BoneSystem, "ALGINE_BONE_SYSTEM")
        constant(Lighting, "ALGINE_LIGHTING_MODE_ENABLED")
        constant(SSR, "ALGINE_SSR_MODE_ENABLED")
        constant(TextureMapping, "ALGINE_TEXTURE_MAPPING_MODE_ENABLED")
        constant(TextureMappingSwitcher, "ALGINE_TEXTURE_MAPPING_MODE_DUAL")
    }

    namespace Vars {
        constant(ModelMatrix, "modelMatrix")
        constant(ViewMatrix, "viewMatrix")
        constant(MVPMatrix, "MVPMatrix")
        constant(MVMatrix, "MVMatrix")
        constant(InPos, "inPos")
        constant(InNormal, "inNormal")
        constant(InTexCoord, "inTexCoord")
        constant(InTangent, "inTangent")
        constant(InBitangent, "inBitangent")

        constant(AmbientTex, "ambient")
        constant(DiffuseTex, "diffuse")
        constant(SpecularTex, "specular")
        constant(NormalTex, "normal")
        constant(ReflectionStrengthTex, "reflectionStrength")
        constant(JitterTex, "jitter")
        constant(AmbientStrength, "ambientStrength")
        constant(DiffuseStrength, "diffuseStrength")
        constant(SpecularStrength, "specularStrength")
        constant(Shininess, "shininess")

        constant(CameraPos, "cameraPos")
    }
}

#undef constant

#endif //COLORSHADER_H
