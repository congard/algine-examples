#include <algine/core/shader/ShaderManager.h>

#include <algine/core/JsonHelper.h>

#include <algine/constants/Material.h>
#include <algine/constants/Lighting.h>
#include <algine/constants/NormalMapping.h>
#include <algine/constants/BoneSystem.h>
#include <algine/constants/ShadowShader.h>
#include <algine/constants/CubemapShader.h>

#include <algine/ext/constants/COCShader.h>
#include <algine/ext/constants/BlendBloom.h>
#include <algine/ext/constants/BlurShader.h>

#include <tulz/File.h>

#include <string>

#include "ColorShader.h"
#include "constants.h"

using namespace std;
using namespace algine;
using namespace tulz;

struct ShaderPair {
    string hor, vert;
};

inline void setIncludePaths(ShaderManager &manager) {
    manager.addIncludePath(algineResources);
    manager.addIncludePath(resources "shaders");
}

string colorShader() {
    ShaderManager manager;

    setIncludePaths(manager);

    manager.fromFile(algineResources "templates/ColorShader/vertex.glsl",
                     algineResources "templates/ColorShader/fragment.glsl");
    manager.define(Module::Material::Settings::IncludeCustomProps);
    manager.define(Module::Lighting::Settings::Attenuation);
    manager.define(Module::Lighting::Settings::ShadowMappingPCF);
    manager.define(Module::Lighting::Settings::PointLightsLimit, pointLightsLimit);
    manager.define(Module::Lighting::Settings::DirLightsLimit, dirLightsLimit);
    manager.define(Module::NormalMapping::Settings::FromMap);
    manager.define(ColorShader::Settings::Lighting);
    manager.define(ColorShader::Settings::TextureMapping);
    manager.define(ColorShader::Settings::SSR);
    manager.define(ColorShader::Settings::BoneSystem);
    manager.define(Module::BoneSystem::Settings::MaxBoneAttribsPerVertex, maxBoneAttribsPerVertex);
    manager.define(Module::BoneSystem::Settings::MaxBones, maxBones);

    return manager.serialize().toString();
}

string pointShadowShader() {
    ShaderManager manager;

    setIncludePaths(manager);

    manager.fromFile(algineResources "shaders/Shadow.vert.glsl",
                     algineResources "shaders/Shadow.frag.glsl",
                     algineResources "shaders/Shadow.geom.glsl");
    manager.resetDefinitions();
    manager.define(Module::BoneSystem::Settings::MaxBoneAttribsPerVertex, maxBoneAttribsPerVertex);
    manager.define(Module::BoneSystem::Settings::MaxBones, maxBones);
    manager.define(ShadowShader::Settings::PointLightShadowMapping);
    manager.define(ShadowShader::Settings::BoneSystem);

    return manager.serialize().toString();
}

string dirShadowShader() {
    ShaderManager manager;

    setIncludePaths(manager);

    manager.fromFile(algineResources "shaders/Shadow.vert.glsl",
                     algineResources "shaders/Shadow.frag.glsl");
    manager.resetDefinitions();
    manager.define(Module::BoneSystem::Settings::MaxBoneAttribsPerVertex, maxBoneAttribsPerVertex);
    manager.define(Module::BoneSystem::Settings::MaxBones, maxBones);
    manager.define(ShadowShader::Settings::DirLightShadowMapping);
    manager.define(ShadowShader::Settings::BoneSystem);

    return manager.serialize().toString();
}

string dofCocShader() {
    ShaderManager manager;

    setIncludePaths(manager);

    manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                     algineResources "shaders/DOFCOC.frag.glsl");
    manager.define(COCShader::Settings::Cinematic);

    return manager.serialize().toString();
}

string blendShader() {
    ShaderManager manager;

    setIncludePaths(manager);

    manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                     algineResources "templates/Blend.frag.glsl");
    manager.define(Module::BlendBloom::Settings::BloomAdd);

    return manager.serialize().toString();
}

ShaderPair bloomBlurShader() {
    ShaderManager manager;

    setIncludePaths(manager);

    manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                     algineResources "shaders/Blur.frag.glsl");
    manager.define(BlurShader::Settings::KernelRadius, bloomBlurKernelRadius);
    manager.define(BlurShader::Settings::OutputType, "vec3");
    manager.define(BlurShader::Settings::TexComponent, "rgb");
    manager.define(BlurShader::Settings::Horizontal);

    ShaderPair result;
    result.hor = manager.serialize().toString();

    manager.removeDefinition(BlurShader::Settings::Horizontal);
    manager.define(BlurShader::Settings::Vertical);

    result.vert = manager.serialize().toString();

    return result;
}

ShaderPair dofBlurShader() {
    ShaderManager manager;

    setIncludePaths(manager);

    manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                     algineResources "shaders/Blur.frag.glsl");
    manager.define(BlurShader::Settings::KernelRadius, dofBlurKernelRadius);
    manager.define(BlurShader::Settings::OutputType, "vec3");
    manager.define(BlurShader::Settings::TexComponent, "rgb");
    manager.define(BlurShader::Settings::Horizontal);

    ShaderPair result;
    result.hor = manager.serialize().toString();

    manager.removeDefinition(BlurShader::Settings::Horizontal);
    manager.define(BlurShader::Settings::Vertical);

    result.vert = manager.serialize().toString();

    return result;
}

ShaderPair cocBlurShader() {
    ShaderManager manager;

    setIncludePaths(manager);

    manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                     algineResources "shaders/Blur.frag.glsl");
    manager.define(BlurShader::Settings::KernelRadius, cocBlurKernelRadius);
    manager.define(BlurShader::Settings::OutputType, "float");
    manager.define(BlurShader::Settings::TexComponent, "r");
    manager.define(BlurShader::Settings::Horizontal);

    ShaderPair result;
    result.hor = manager.serialize().toString();

    manager.removeDefinition(BlurShader::Settings::Horizontal);
    manager.define(BlurShader::Settings::Vertical);

    result.vert = manager.serialize().toString();

    return result;
}

string cubeMapShader() {
    ShaderManager manager;

    setIncludePaths(manager);

    manager.fromFile(algineResources "shaders/basic/Cubemap.vert.glsl",
                     algineResources "shaders/basic/Cubemap.frag.glsl");
    manager.define(CubemapShader::Settings::SpherePositions);
    manager.define(CubemapShader::Settings::ColorOut, "0"); // TODO: create constants
    manager.define(CubemapShader::Settings::PosOut, "2");
    manager.define(CubemapShader::Settings::OutputType, "vec3");

    return manager.serialize().toString();
}

string ssrShader() {
    ShaderManager manager;

    manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                     algineResources "shaders/SSR.frag.glsl");

    return manager.serialize().toString();
}

string bloomSearchShader() {
    ShaderManager manager;

    manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                     algineResources "shaders/BloomSearch.frag.glsl");

    return manager.serialize().toString();
}

#define writeShaderConfig(name) File(resources "shaders/" #name ".conf.json", File::WriteText).write(name())

#define writePairShaderConfigBase(name, inf) \
    File(resources "shaders/" #name "." #inf ".conf.json", File::WriteText).write(data.inf)

#define writePairShaderConfig(name) \
    { \
        auto data = name(); \
        writePairShaderConfigBase(name, hor); \
        writePairShaderConfigBase(name, vert); \
    }

int main() {
    writeShaderConfig(colorShader);
    writeShaderConfig(pointShadowShader);
    writeShaderConfig(dirShadowShader);
    writeShaderConfig(dofCocShader);
    writeShaderConfig(blendShader);
    writeShaderConfig(cubeMapShader);
    writeShaderConfig(ssrShader);
    writeShaderConfig(bloomSearchShader);

    writePairShaderConfig(bloomBlurShader)
    writePairShaderConfig(dofBlurShader)
    writePairShaderConfig(cocBlurShader)

    return 0;
}
