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

#include <algine/std/AMTLMaterialManager.h>
#include <algine/std/AMTLManager.h>

#include <tulz/File.h>

#include <string>

#include "ColorShader.h"
#include "constants.h"

using namespace std;
using namespace algine;
using namespace tulz;

using TextureType = AMTLMaterialManager::Texture;

#define ext ".conf.json"

struct ShaderPair {
    string horizontal, vertical;
};

inline void write(const string &name, const string &data) {
    File(resources + name + ext, File::WriteText).write(data);
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

string ShadowVertGLSL = "Shadow.vert";
string QuadVertGLSL = "Quad.vert";
string ShadowVertGLSLPath = resources + shader(ShadowVertGLSL) + ext;
string QuadVertGLSLPath = resources + shader(QuadVertGLSL) + ext;

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

    manager.setShaders({vertex, fragment});

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

    manager.addShaderPath(ShadowVertGLSLPath);
    manager.setShaders({fragment, geometry});

    return manager.dump().toString();
}

string dirShadowProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/Shadow.frag.glsl");

    ShaderProgramManager manager;
    manager.define(ShadowShader::Settings::DirLightShadowMapping);

    manager.addShaderPath(ShadowVertGLSLPath);
    manager.addShader(fragment);

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

    manager.addShaderPath(QuadVertGLSLPath);
    manager.addShader(fragment);

    return manager.dump().toString();
}

string blendProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "templates/Blend.frag.glsl");

    ShaderProgramManager manager;
    manager.define(Module::BlendBloom::Settings::BloomAdd);

    manager.addShaderPath(QuadVertGLSLPath);
    manager.addShader(fragment);

    return manager.dump().toString();
}

string ssrProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/SSR.frag.glsl");

    ShaderProgramManager manager;
    manager.addShaderPath(QuadVertGLSLPath);
    manager.addShader(fragment);

    return manager.dump().toString();
}

string bloomSearchProgram() {
    ShaderManager fragment(Shader::Fragment);
    fragment.fromFile(algineResources "shaders/BloomSearch.frag.glsl");

    ShaderProgramManager manager;
    manager.addShaderPath(QuadVertGLSLPath);
    manager.addShader(fragment);

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

    manager.addShaderPath(QuadVertGLSLPath);
    manager.addShader(fragment);

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

    manager.addShaderPath(QuadVertGLSLPath);
    manager.addShader(fragment);

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

    manager.addShaderPath(QuadVertGLSLPath);
    manager.addShader(fragment);

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

    manager.setShaders({vertex, fragment});

    return manager.dump().toString();
}

string skyboxTexture() {
    TextureCubeManager manager;
    manager.setPath("right.tga", TextureCube::Face::Right);
    manager.setPath("left.tga", TextureCube::Face::Left);
    manager.setPath("top.jpg", TextureCube::Face::Top);
    manager.setPath("bottom.tga", TextureCube::Face::Bottom);
    manager.setPath("front.tga", TextureCube::Face::Front);
    manager.setPath("back.tga", TextureCube::Face::Back);

    return manager.dump().toString();
}

namespace Astroboy {
string texture() {
    Texture2DManager manager;
    manager.setName("astroboy_texture");
    manager.setAccess(ManagerBase::Access::Public);
    manager.setPath("boy.jpg");

    return manager.dump().toString();
}

string normalMap() {
    Texture2DManager manager;
    manager.setName("astroboy_normals");
    manager.setAccess(ManagerBase::Access::Public);
    manager.setPath("norm.png");

    return manager.dump().toString();
}

string materialLibrary() {
    AMTLMaterialManager materialManager;
    materialManager.setTexturePaths({
        {TextureType::Ambient, "boy" ext},
        {TextureType::Diffuse, "boy" ext},
        {TextureType::Specular, "boy" ext},
        {TextureType::Normal, "norm" ext}
    });

    AMTLManager manager;
    manager.addMaterial(materialManager, "face");
    manager.addMaterial(materialManager, "glass");
    manager.addMaterial(materialManager, "shiny");
    manager.addMaterial(materialManager, "matte");

    return manager.dump().toString();
}
}

namespace Chess {
inline Texture2DManager getTexture(const string &path, const string &name = {},
                         ManagerBase::Access access = ManagerBase::Access::Private)
{
    Texture2DManager manager;
    manager.setPath(path);
    manager.setParams({
        {Texture::WrapU, Texture::Repeat},
        {Texture::WrapV, Texture::Repeat},
        {Texture::MinFilter, Texture::Linear},
        {Texture::MagFilter, Texture::Linear}
    });
    manager.setAccess(access);
    manager.setName(name);

    return manager;
}

string blackTexture() {
    return getTexture("Brown_wood_texture.jpg", "Brown_wood_texture", ManagerBase::Access::Public).dump().toString();
}

auto blackSpecularMap() {
    return getTexture("Brown_wood_texture_SPEC.jpg");
}

auto blackNormalMap() {
    return getTexture("Brown_wood_texture_NORM.png");
}

string whiteTexture() {
    return getTexture("White_wood_texture.jpg", "White_wood_texture", ManagerBase::Access::Public).dump().toString();
}

auto whiteSpecularMap() {
    return getTexture("White_wood_texture_SPEC.jpg");
}

auto whiteNormalMap() {
    return getTexture("White_wood_texture_NORM.png");
}

auto reflectionTexture() {
    return getTexture("desk_texture_reflection_strength_jitter.jpg");
}

auto jitterTexture() {
    return getTexture("jitter.jpg");
}

string materialLibrary() {
    AMTLMaterialManager deskMaterial;
    deskMaterial.setTextures({
        {TextureType::Reflection, reflectionTexture()},
        {TextureType::Jitter, jitterTexture()}
    });

    AMTLMaterialManager blackMaterial;
    blackMaterial.setTexturePaths({
        {TextureType::Ambient, "Brown_wood_texture" ext},
        {TextureType::Diffuse, "Brown_wood_texture" ext}
    });
    blackMaterial.setTextures({
        {TextureType::Specular, blackSpecularMap()},
        {TextureType::Normal, blackNormalMap()}
    });

    AMTLMaterialManager whiteMaterial;
    whiteMaterial.setTexturePaths({
        {TextureType::Ambient, "White_wood_texture" ext},
        {TextureType::Diffuse, "White_wood_texture" ext}
    });
    whiteMaterial.setTextures({
        {TextureType::Specular, whiteSpecularMap()},
        {TextureType::Normal, whiteNormalMap()}
    });

    AMTLManager manager;
    manager.addMaterial(deskMaterial, "desk");
    manager.addMaterial(blackMaterial, "black");
    manager.addMaterial(whiteMaterial, "white");

    return manager.dump().toString();
}
}

namespace Man {
string texture() {
    Texture2DManager manager;
    manager.setName("man_texture");
    manager.setAccess(ManagerBase::Access::Public);
    manager.setPath("tex.jpg");

    return manager.dump().toString();
}

string normalMap() {
    Texture2DManager manager;
    manager.setName("man_normals");
    manager.setAccess(ManagerBase::Access::Public);
    manager.setPath("norm.png");

    return manager.dump().toString();
}

string materialLibrary() {
    AMTLMaterialManager materialManager;
    materialManager.setTexturePaths({
        {TextureType::Ambient, "tex" ext},
        {TextureType::Diffuse, "tex" ext},
        {TextureType::Specular, "tex" ext},
        {TextureType::Normal, "norm" ext}
    });

    AMTLManager manager;
    manager.addMaterial(materialManager, "material");

    return manager.dump().toString();
}
}

int main() {
    // shaders & programs
    write(shader(ShadowVertGLSL), vertexShadowShader());
    write(program("PointShadow"), pointShadowProgram());
    write(program("DirShadow"), dirShadowProgram());

    write(shader(QuadVertGLSL), quadVertexShader());
    write(program("DofCoc"), dofCocProgram());
    write(program("Blend"), blendProgram());
    write(program("SSR"), ssrProgram());
    write(program("BloomSearch"), bloomSearchProgram());
    write(program("BloomBlur"), bloomBlurProgram());
    write(program("DofBlur"), dofBlurProgram());
    write(program("CocBlur"), cocBlurProgram());

    write(program("Color"), colorProgram());
    write(program("Skybox"), skyboxProgram());

    // textures & materials
    write(texture("skybox/Skybox"), skyboxTexture());

    write("models/astroboy/boy", Astroboy::texture());
    write("models/astroboy/norm", Astroboy::normalMap());
    File(resources "models/astroboy/astroboy_walk.amtl", File::WriteText).write(Astroboy::materialLibrary());

    write("models/chess/Brown_wood_texture", Chess::blackTexture());
    write("models/chess/White_wood_texture", Chess::whiteTexture());
    File(resources "models/chess/Classic Chess small.amtl", File::WriteText).write(Chess::materialLibrary());

    write("models/man/tex", Man::texture());
    write("models/man/norm", Man::normalMap());
    File(resources "models/man/man.amtl", File::WriteText).write(Man::materialLibrary());

    return 0;
}
