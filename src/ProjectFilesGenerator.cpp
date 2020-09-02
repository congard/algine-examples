#include <algine/core/shader/ShaderManager.h>
#include <algine/core/shader/ShaderProgramManager.h>

#include <algine/core/texture/TextureCubeManager.h>

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
    string horizontal, vertical;
};

string colorProgram() {
    ShaderManager vertex(Shader::Vertex);
    vertex.fromFile(algineResources "templates/ColorShader/vertex.glsl");

    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "templates/ColorShader/fragment.glsl");

    ShaderProgramManager manager;
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

    manager.setPrivateShaders({vertex, fragment});

    return manager.dump().toString();
}

string vertexShadowShader() {
    ShaderManager vertex(Shader::Vertex);
    vertex.fromFile(algineResources "shaders/Shadow.vert.glsl");
    vertex.setAccess(ShaderManager::Access::Public);
    vertex.setName("ShadowVertexShader");

    vertex.define(Module::BoneSystem::Settings::MaxBoneAttribsPerVertex, maxBoneAttribsPerVertex);
    vertex.define(Module::BoneSystem::Settings::MaxBones, maxBones);
    vertex.define(ShadowShader::Settings::BoneSystem);

    return vertex.dump().toString();
}

string pointShadowProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/Shadow.frag.glsl");

    ShaderManager geometry(Shader::Geometry);
    geometry.fromFile(algineResources "shaders/Shadow.geom.glsl");

    ShaderProgramManager manager;
    manager.define(ShadowShader::Settings::PointLightShadowMapping);

    manager.addPublicShader("ShadowVertexShader");
    manager.setPrivateShaders({fragment, geometry});

    return manager.dump().toString();
}

string dirShadowProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/Shadow.frag.glsl");

    ShaderProgramManager manager;
    manager.define(ShadowShader::Settings::DirLightShadowMapping);

    manager.addPublicShader("ShadowVertexShader");
    manager.addPrivateShader(fragment);

    return manager.dump().toString();
}

string quadVertexShader() {
    ShaderManager vertex(Shader::Vertex);
    vertex.fromFile(algineResources "shaders/basic/Quad.vert.glsl");
    vertex.setAccess(ShaderManager::Access::Public);
    vertex.setName("QuadVertexShader");

    return vertex.dump().toString();
}

string dofCocProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/DOFCOC.frag.glsl");

    ShaderProgramManager manager;
    manager.define(COCShader::Settings::Cinematic);

    manager.addPublicShader("QuadVertexShader");
    manager.addPrivateShader(fragment);

    return manager.dump().toString();
}

string blendProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "templates/Blend.frag.glsl");

    ShaderProgramManager manager;
    manager.define(Module::BlendBloom::Settings::BloomAdd);

    manager.addPublicShader("QuadVertexShader");
    manager.addPrivateShader(fragment);

    return manager.dump().toString();
}

string ssrProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/SSR.frag.glsl");

    ShaderProgramManager manager;
    manager.addPublicShader("QuadVertexShader");
    manager.addPrivateShader(fragment);

    return manager.dump().toString();
}

string bloomSearchProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/BloomSearch.frag.glsl");

    ShaderProgramManager manager;
    manager.addPublicShader("QuadVertexShader");
    manager.addPrivateShader(fragment);

    return manager.dump().toString();
}

ShaderPair bloomBlurProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/Blur.frag.glsl");

    ShaderProgramManager manager;
    manager.define(BlurShader::Settings::KernelRadius, bloomBlurKernelRadius);
    manager.define(BlurShader::Settings::OutputType, "vec3");
    manager.define(BlurShader::Settings::TexComponent, "rgb");
    manager.define(BlurShader::Settings::Horizontal);

    manager.addPublicShader("QuadVertexShader");
    manager.addPrivateShader(fragment);

    ShaderPair result;
    result.horizontal = manager.dump().toString();

    manager.removeDefinition(BlurShader::Settings::Horizontal);
    manager.define(BlurShader::Settings::Vertical);

    result.vertical = manager.dump().toString();

    return result;
}

ShaderPair dofBlurProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/Blur.frag.glsl");

    ShaderProgramManager manager;
    manager.define(BlurShader::Settings::KernelRadius, dofBlurKernelRadius);
    manager.define(BlurShader::Settings::OutputType, "vec3");
    manager.define(BlurShader::Settings::TexComponent, "rgb");
    manager.define(BlurShader::Settings::Horizontal);

    manager.addPublicShader("QuadVertexShader");
    manager.addPrivateShader(fragment);

    ShaderPair result;
    result.horizontal = manager.dump().toString();

    manager.removeDefinition(BlurShader::Settings::Horizontal);
    manager.define(BlurShader::Settings::Vertical);

    result.vertical = manager.dump().toString();

    return result;
}

ShaderPair cocBlurProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/Blur.frag.glsl");

    ShaderProgramManager manager;
    manager.define(BlurShader::Settings::KernelRadius, cocBlurKernelRadius);
    manager.define(BlurShader::Settings::OutputType, "float");
    manager.define(BlurShader::Settings::TexComponent, "r");
    manager.define(BlurShader::Settings::Horizontal);

    manager.addPublicShader("QuadVertexShader");
    manager.addPrivateShader(fragment);

    ShaderPair result;
    result.horizontal = manager.dump().toString();

    manager.removeDefinition(BlurShader::Settings::Horizontal);
    manager.define(BlurShader::Settings::Vertical);

    result.vertical = manager.dump().toString();

    return result;
}

string skyboxProgram() {
    ShaderManager vertex(Shader::Vertex);
    vertex.fromFile(algineResources "shaders/basic/Cubemap.vert.glsl");

    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/basic/Cubemap.frag.glsl");

    ShaderProgramManager manager;
    manager.define(CubemapShader::Settings::SpherePositions);
    manager.define(CubemapShader::Settings::ColorOut, "0"); // TODO: create constants
    manager.define(CubemapShader::Settings::PosOut, "2");
    manager.define(CubemapShader::Settings::OutputType, "vec3");

    manager.setPrivateShaders({vertex, fragment});

    return manager.dump().toString();
}

inline void write(const string &name, const string &data) {
    File(resources + name + ".conf.json", File::WriteText).write(data);
}

inline void write(const string &name, const ShaderPair &data) {
    write(name + ".ver", data.vertical);
    write(name + ".hor", data.horizontal);
}

inline string shader(const string &name) {
    return "shaders/" + name;
}

inline string program(const string &name) {
    return "programs/" + name;
}

inline string texture(const string &name) {
    return "textures/" + name;
}

string skyboxTexture() {
    TextureCubeManager manager;
    manager.setPath(resources "skybox/right.tga", TextureCube::Face::Right);
    manager.setPath(resources "skybox/left.tga", TextureCube::Face::Left);
    manager.setPath(resources "skybox/top.jpg", TextureCube::Face::Top);
    manager.setPath(resources "skybox/bottom.tga", TextureCube::Face::Bottom);
    manager.setPath(resources "skybox/front.tga", TextureCube::Face::Front);
    manager.setPath(resources "skybox/back.tga", TextureCube::Face::Back);

    return manager.dump().toString();
}

int main() {
    // shaders & programs
    write(shader("Shadow.vert"), vertexShadowShader());
    write(program("PointShadow"), pointShadowProgram());
    write(program("DirShadow"), dirShadowProgram());

    write(shader("Quad.vert"), quadVertexShader());
    write(program("DofCoc"), dofCocProgram());
    write(program("Blend"), blendProgram());
    write(program("SSR"), ssrProgram());
    write(program("BloomSearch"), bloomSearchProgram());
    write(program("BloomBlur"), bloomBlurProgram());
    write(program("DofBlur"), dofBlurProgram());
    write(program("CocBlur"), cocBlurProgram());

    write(program("Color"), colorProgram());
    write(program("Skybox"), skyboxProgram());

    // textures
    write(texture("Skybox"), skyboxTexture());

    return 0;
}
