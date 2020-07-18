/**
 * The scene with chess desk and 2 running humanoids
 * @author congard
 */

#define GLM_FORCE_CTOR_INIT

#include <iostream>
#include <thread>
#include <chrono>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <tulz/Path.h>

#include <algine/core/Engine.h>
#include <algine/core/shader/ShaderProgram.h>
#include <algine/core/shader/ShaderManager.h>
#include <algine/core/Framebuffer.h>
#include <algine/core/Renderbuffer.h>
#include <algine/core/texture/Texture2D.h>
#include <algine/core/texture/TextureCube.h>
#include <algine/core/texture/TextureTools.h>

#include <algine/std/camera/Camera.h>
#include <algine/std/camera/FPSCameraController.h>
#include <algine/std/rotator/EulerRotator.h>
#include <algine/std/model/Model.h>
#include <algine/std/model/ShapeLoader.h>
#include <algine/std/lighting/DirLamp.h>
#include <algine/std/lighting/PointLamp.h>
#include <algine/std/lighting/Manager.h>
#include <algine/std/CubeRenderer.h>
#include <algine/std/QuadRenderer.h>
#include <algine/std/animation/AnimationBlender.h>

#include <algine/ext/debug.h>
#include <algine/ext/Blur.h>
#include <algine/ext/event/MouseEvent.h>
#include <algine/ext/constants/BlurShader.h>
#include <algine/ext/constants/SSRShader.h>
#include <algine/ext/constants/BlendBloom.h>
#include <algine/ext/constants/COCShader.h>
#include <algine/ext/constants/BlendDOF.h>

#include <algine/constants/BoneSystem.h>
#include <algine/constants/ShadowShader.h>
#include <algine/constants/CubemapShader.h>
#include <algine/constants/Material.h>
#include <algine/constants/Lighting.h>
#include <algine/constants/NormalMapping.h>

#include "ColorShader.h"
#include "BlendShader.h"

using namespace algine;
using namespace std;
using namespace tulz;

using Lighting::PointLamp;
using Lighting::DirLamp;

constexpr bool fullscreen = false;
constexpr uint shadowMapResolution = 1024;

constexpr float bloomK = 0.5f;
constexpr float dofK = 0.5f;
constexpr uint bloomBlurKernelRadius = 15;
constexpr uint bloomBlurKernelSigma = 16;
constexpr uint dofBlurKernelRadius = 2;
constexpr uint dofBlurKernelSigma = 4;
constexpr uint cocBlurKernelRadius = 2;
constexpr uint cocBlurKernelSigma = 6;

constexpr uint pointLightsCount = 1;
constexpr uint dirLightsCount = 1;
constexpr uint maxBoneAttribsPerVertex = 1;
constexpr uint maxBones = 64;
constexpr uint shapesCount = 4;
constexpr uint modelsCount = 3;

#define algineResources "lib/algine/resources/"
#define resources "src/resources/"

// Function prototypes
void key_callback(GLFWwindow* glfwWindow, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* glfwWindow, int button, int action, int mods);
void mouse_callback(MouseEvent *event);
void cursor_pos_callback(GLFWwindow* glfwWindow, double x, double y);

// Window dimensions
uint winWidth = 1366, winHeight = 763;
GLFWwindow* window;

// matrices
const glm::mat4 *modelMatrix; // model matrix stored in Model::transformation

// models
shared_ptr<Shape> shapes[shapesCount];
Model models[modelsCount], lamps[pointLightsCount + dirLightsCount];
Animator manAnimator, astroboyAnimator; // animator for man, astroboy models
AnimationBlender manAnimationBlender;

// light
PointLamp pointLamps[pointLightsCount];
DirLamp dirLamps[dirLightsCount];
Lighting::Manager lightManager;

shared_ptr<CubeRenderer> skyboxRenderer;
shared_ptr<QuadRenderer> quadRenderer;

shared_ptr<Blur> bloomBlur;
shared_ptr<Blur> dofBlur;
shared_ptr<Blur> cocBlur;

Renderbuffer *rbo;
Framebuffer *displayFb;
Framebuffer *screenspaceFb;
Framebuffer *bloomSearchFb;
Framebuffer *cocFb;

Texture2D *colorTex;
Texture2D *normalTex;
Texture2D *ssrValues;
Texture2D *positionTex;
Texture2D *screenspaceTex;
Texture2D *bloomTex;
Texture2D *cocTex;
TextureCube *skybox;

ShaderProgram *skyboxShader;
ShaderProgram *colorShader;
ShaderProgram *pointShadowShader;
ShaderProgram *dirShadowShader;
ShaderProgram *dofBlurHorShader, *dofBlurVertShader;
ShaderProgram *dofCoCShader;
ShaderProgram *ssrShader;
ShaderProgram *bloomSearchShader;
ShaderProgram *bloomBlurHorShader, *bloomBlurVertShader;
ShaderProgram *cocBlurHorShader, *cocBlurVertShader;
ShaderProgram *blendShader;

MouseEventListener mouseEventListener;

Camera camera;
FPSCameraController camController;

EulerRotator manHeadRotator;

constexpr float blendExposure = 6.0f, blendGamma = 1.125f;

constexpr float
    // DOF variables
    dofImageDistance = 1.0f,
    dofAperture = 10.0f,
    dofSigmaDivider = 1.5f,

    // diskRadius variables
    diskRadius_k = 1.0f / 25.0f,
    diskRadius_min = 0.0f,

    // shadow opacity: 1.0 - opaque shadow (by default), 0.0 - transparent
    shadowOpacity = 0.65f;

inline void updateTexture(Texture *const texture, const uint width, const uint height) {
    texture->setWidthHeight(width, height);
    texture->bind();
    texture->update();
}

void updateRenderTextures() {
    updateTexture(colorTex, winWidth, winHeight);
    updateTexture(normalTex, winWidth, winHeight);
    updateTexture(ssrValues, winWidth, winHeight);
    updateTexture(positionTex, winWidth, winHeight);
    updateTexture(screenspaceTex, winWidth, winHeight); // TODO: make small sst + blend pass in the future
    updateTexture(bloomTex, winWidth * bloomK, winHeight * bloomK);

    for (size_t i = 0; i < 2; ++i) {
        updateTexture(bloomBlur->getPingPongTextures()[i], winWidth * bloomK, winHeight * bloomK);
        updateTexture(cocBlur->getPingPongTextures()[i], winWidth * dofK, winHeight * dofK);
        updateTexture(dofBlur->getPingPongTextures()[i], winWidth * dofK, winHeight * dofK);
    }

    updateTexture(cocTex, winWidth * dofK, winHeight * dofK);
}

/**
 * To correctly display the scene when changing the window size
 */
void window_size_callback(GLFWwindow* window, int width, int height) {
    winWidth = width;
    winHeight = height;

    rbo->bind();
    rbo->setWidthHeight(width, height);
    rbo->update();
    rbo->unbind();

    updateRenderTextures();

    camera.setAspectRatio((float)winWidth / (float)winHeight);
    camera.perspective();
}

/**
 * Creates Lamp with default params
 */
void createPointLamp(PointLamp &result, const glm::vec3 &pos, const glm::vec3 &color, usize id) {
    result.setPos(pos);
    result.translate();
    result.setColor(color);
    result.setKc(1.0f);
    result.setKl(0.045f);
    result.setKq(0.0075f);

    result.perspectiveShadows();
    result.updateMatrix();
    result.initShadows(shadowMapResolution, shadowMapResolution);

    colorShader->bind();
    lightManager.transmitter.setColor(result, id);
    lightManager.transmitter.setKc(result, id);
    lightManager.transmitter.setKl(result, id);
    lightManager.transmitter.setKq(result, id);
    lightManager.transmitter.setFarPlane(result, id);
    lightManager.transmitter.setBias(result, id);
    colorShader->unbind();
}

void createDirLamp(DirLamp &result,
        const glm::vec3 &pos, const glm::vec3 &rotate,
        const glm::vec3 &color,
        usize id) {
    result.setPos(pos);
    result.translate();
    result.setRotate(rotate.y, rotate.x, rotate.z);
    result.rotate();
    result.updateMatrix();

    result.setColor(color);
    result.setKc(1.0f);
    result.setKl(0.045f);
    result.setKq(0.0075f);

    result.orthoShadows(-10.0f, 10.0f, -10.0f, 10.0f);
    result.updateMatrix();
    result.initShadows(shadowMapResolution, shadowMapResolution);

    colorShader->bind();
    lightManager.transmitter.setColor(result, id);
    lightManager.transmitter.setKc(result, id);
    lightManager.transmitter.setKl(result, id);
    lightManager.transmitter.setKq(result, id);
    lightManager.transmitter.setMinBias(result, id);
    lightManager.transmitter.setMaxBias(result, id);
    lightManager.transmitter.setLightMatrix(result, id);
    colorShader->unbind();
}

void
createShapes(const string &path, const string &texPath, const size_t id, const bool inverseNormals = false,
             uint bonesPerVertex = 0) {
    ShapeLoader shapeLoader;
    shapeLoader.setModelPath(path);
    shapeLoader.setTexturesPath(texPath);
    if (inverseNormals)
        shapeLoader.addParam(ShapeLoader::InverseNormals);
    shapeLoader.addParams(ShapeLoader::Triangulate, ShapeLoader::SortByPolygonType,
            ShapeLoader::CalcTangentSpace, ShapeLoader::JoinIdenticalVertices, ShapeLoader::PrepareAllAnimations);
    shapeLoader.getShape()->bonesPerVertex = bonesPerVertex;
    shapeLoader.load();

    shapes[id].reset(shapeLoader.getShape());

    {
        InputLayoutShapeLocations locations; // shadow shaders locations
        locations.inPosition = pointShadowShader->getLocation(ShadowShader::Vars::InPos);
        locations.inBoneWeights = pointShadowShader->getLocation(BoneSystem::Vars::InBoneWeights);
        locations.inBoneIds = pointShadowShader->getLocation(BoneSystem::Vars::InBoneIds);
        shapes[id]->createInputLayout(locations); // all shadow shaders have same ids
    }

    {
        InputLayoutShapeLocations locations; // color shader locations
        locations.inPosition = colorShader->getLocation(ColorShader::Vars::InPos);
        locations.inTexCoord = colorShader->getLocation(ColorShader::Vars::InTexCoord);
        locations.inNormal = colorShader->getLocation(ColorShader::Vars::InNormal);
        locations.inTangent = colorShader->getLocation(ColorShader::Vars::InTangent);
        locations.inBitangent = colorShader->getLocation(ColorShader::Vars::InBitangent);
        locations.inBoneWeights = colorShader->getLocation(BoneSystem::Vars::InBoneWeights);
        locations.inBoneIds = colorShader->getLocation(BoneSystem::Vars::InBoneIds);
        shapes[id]->createInputLayout(locations);
    }
}

/* init code begin */

// NOTE: uncomment if you have issues
// send report to mailto:dbcongard@gmail.com
// or create new issue: https://github.com/congard/algine
// #define DEBUG_OUTPUT

/**
 * Init GL: creating window, etc.
 */
void initGL() {
    std::cout << "Starting GLFW context, OpenGL 3.3" << std::endl;
    // Init GLFW
    if (!glfwInit())
        std::cout << "GLFW init failed";
    // Set all the required options for GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
#ifdef DEBUG_OUTPUT
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    // Create a GLFWwindow object that we can use for GLFW's functions
    window = fullscreen ?
             glfwCreateWindow(winWidth, winHeight, "Algine", glfwGetPrimaryMonitor(), nullptr) :
             glfwCreateWindow(winWidth, winHeight, "Algine", nullptr, nullptr);

    glfwMakeContextCurrent(window);

    // Set the required callback functions
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);

    // Set this to true so GLEW knows to use a modern approach to retrieving function pointers and extensions
    glewExperimental = GL_TRUE;
    // Initialize GLEW to setup the OpenGL Function pointers
    if (glewInit() != GLEW_NO_ERROR)
        std::cout << "GLEW init failed\n";

#ifdef DEBUG_OUTPUT
    enableGLDebugOutput();
#endif

    std::cout << "Your GPU vendor: " << Engine::getGPUVendor() << "\nYour GPU renderer: " << Engine::getGPURenderer() << "\n";

    Engine::init();
    Engine::enableDepthTest();
    Engine::enableDepthMask();
    Engine::enableFaceCulling();

    // SOP test
//    auto test1 = new Framebuffer();
//    auto test2 = new Framebuffer();
//    test1->bind();
//    test2->unbind();
}

/**
 * Loading and compiling shaders
 */
void initShaders() {
    ShaderProgram::create(skyboxShader, colorShader, pointShadowShader, dirShadowShader,
                          dofBlurHorShader, dofBlurVertShader, dofCoCShader, ssrShader,
                          bloomSearchShader, bloomBlurHorShader, bloomBlurVertShader,
                          cocBlurHorShader, cocBlurVertShader, blendShader);

    lightManager.setDirLightsLimit(4);
    lightManager.setPointLightsLimit(4);
    lightManager.setPointLightsMapInitialSlot(6);
    lightManager.setDirLightsMapInitialSlot(lightManager.pointLightsInitialSlot + lightManager.pointLightsLimit);

    std::cout << "Compiling algine shaders\n";

    {
        ShaderManager manager;
        manager.addIncludePath(Path::join(Path::getWorkingDirectory(), algineResources));
        manager.addIncludePath(Path::join(Path::getWorkingDirectory(), resources "shaders"));

        // color shader
        manager.fromFile(algineResources "templates/ColorShader/vertex.glsl",
                         algineResources "templates/ColorShader/fragment.glsl");
        manager.define(Module::Material::Settings::IncludeCustomProps);
        manager.define(Module::Lighting::Settings::Attenuation);
        manager.define(Module::Lighting::Settings::ShadowMappingPCF);
        manager.define(Module::Lighting::Settings::PointLightsLimit, std::to_string(lightManager.pointLightsLimit));
        manager.define(Module::Lighting::Settings::DirLightsLimit, std::to_string(lightManager.dirLightsLimit));
        manager.define(Module::NormalMapping::Settings::FromMap);
        manager.define(ColorShader::Settings::Lighting);
        manager.define(ColorShader::Settings::TextureMapping);
        manager.define(ColorShader::Settings::SSR);
        manager.define(ColorShader::Settings::BoneSystem);
        manager.define(BoneSystem::Settings::MaxBoneAttribsPerVertex, std::to_string(maxBoneAttribsPerVertex));
        manager.define(BoneSystem::Settings::MaxBones, std::to_string(maxBones));
        colorShader->fromSource(manager.makeGenerated());
        colorShader->loadActiveLocations();

        // point shadow shader
        manager.fromFile(algineResources "shaders/Shadow.vert.glsl",
                         algineResources "shaders/Shadow.frag.glsl",
                         algineResources "shaders/Shadow.geom.glsl");
        manager.resetDefinitions();
        manager.define(BoneSystem::Settings::MaxBoneAttribsPerVertex, std::to_string(maxBoneAttribsPerVertex));
        manager.define(BoneSystem::Settings::MaxBones, std::to_string(maxBones));
        manager.define(ShadowShader::Settings::PointLightShadowMapping);
        manager.define(ShadowShader::Settings::BoneSystem);
        pointShadowShader->fromSource(manager.makeGenerated());
        pointShadowShader->loadActiveLocations();

        // dir shadow shader
        ShadersInfo shadowShaderTemplate = manager.getTemplate();
        shadowShaderTemplate.geometry = std::string(); // we don't need geometry shader for dir light shadows
        manager.fromSource(shadowShaderTemplate);
        manager.removeDefinition(ShadowShader::Settings::PointLightShadowMapping);
        manager.define(ShadowShader::Settings::DirLightShadowMapping);
        dirShadowShader->fromSource(manager.makeGenerated());
        dirShadowShader->loadActiveLocations();

        // SSR shader
        ssrShader->fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                            algineResources "shaders/SSR.frag.glsl");
        ssrShader->loadActiveLocations();

        // bloom search shader
        bloomSearchShader->fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                                    algineResources "shaders/BloomSearch.frag.glsl");
        bloomSearchShader->loadActiveLocations();

        // DOF CoC shader
        manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                         algineResources "shaders/DOFCOC.frag.glsl");
        manager.resetDefinitions();
        manager.define(COCShader::Settings::Cinematic);
        dofCoCShader->fromSource(manager.makeGenerated());
        dofCoCShader->loadActiveLocations();

        // blend shader
        manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                         algineResources "templates/Blend.frag.glsl");
        manager.resetDefinitions();
        manager.define(Module::BlendBloom::Settings::BloomAdd);
        blendShader->fromSource(manager.makeGenerated());
        blendShader->loadActiveLocations();

        // bloom blur shaders
        manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                         algineResources "shaders/Blur.frag.glsl");
        manager.resetDefinitions();
        manager.define(BlurShader::Settings::KernelRadius, std::to_string(bloomBlurKernelRadius));
        manager.define(BlurShader::Settings::OutputType, "vec3");
        manager.define(BlurShader::Settings::TexComponent, "rgb");
        manager.define(BlurShader::Settings::Horizontal);
        bloomBlurHorShader->fromSource(manager.makeGenerated());
        bloomBlurHorShader->loadActiveLocations();

        manager.resetGenerated();
        manager.removeDefinition(BlurShader::Settings::Horizontal);
        manager.define(BlurShader::Settings::Vertical);
        bloomBlurVertShader->fromSource(manager.makeGenerated());
        bloomBlurVertShader->loadActiveLocations();

        // dof blur shaders
        manager.fromFile(algineResources "shaders/basic/Quad.vert.glsl",
                         algineResources "shaders/Blur.frag.glsl");
        manager.resetDefinitions();
        manager.define(BlurShader::Settings::KernelRadius, std::to_string(dofBlurKernelRadius));
        manager.define(BlurShader::Settings::OutputType, "vec3");
        manager.define(BlurShader::Settings::TexComponent, "rgb");
        manager.define(BlurShader::Settings::Horizontal);
        dofBlurHorShader->fromSource(manager.makeGenerated());
        dofBlurHorShader->loadActiveLocations();

        manager.resetGenerated();
        manager.removeDefinition(BlurShader::Settings::Horizontal);
        manager.define(BlurShader::Settings::Vertical);
        dofBlurVertShader->fromSource(manager.makeGenerated());
        dofBlurVertShader->loadActiveLocations();

        // CoC blur shaders
        manager.resetGenerated();
        manager.resetDefinitions();
        manager.define(BlurShader::Settings::KernelRadius, std::to_string(cocBlurKernelRadius));
        manager.define(BlurShader::Settings::OutputType, "float");
        manager.define(BlurShader::Settings::TexComponent, "r");
        manager.define(BlurShader::Settings::Horizontal);
        cocBlurHorShader->fromSource(manager.makeGenerated());
        cocBlurHorShader->loadActiveLocations();

        manager.resetGenerated();
        manager.removeDefinition(BlurShader::Settings::Horizontal);
        manager.define(BlurShader::Settings::Vertical);
        cocBlurVertShader->fromSource(manager.makeGenerated());
        cocBlurVertShader->loadActiveLocations();

        // cubemap shader
        manager.fromFile(algineResources "shaders/basic/Cubemap.vert.glsl",
                         algineResources "shaders/basic/Cubemap.frag.glsl");
        manager.resetDefinitions();
        manager.define(CubemapShader::Settings::SpherePositions);
        manager.define(CubemapShader::Settings::ColorOut, "0"); // TODO: create constants
        manager.define(CubemapShader::Settings::PosOut, "2");
        manager.define(CubemapShader::Settings::OutputType, "vec3");
        skyboxShader->fromSource(manager.makeGenerated());
        skyboxShader->loadActiveLocations();
    }

    std::cout << "Compilation done\n";

    lightManager.setLightShader(colorShader);
    lightManager.setPointLightShadowShader(pointShadowShader);
    lightManager.indexDirLightLocations();
    lightManager.indexPointLightLocations();

    skyboxRenderer = make_shared<CubeRenderer>(skyboxShader->getLocation(CubemapShader::Vars::InPos));
    quadRenderer = make_shared<QuadRenderer>(0); // inPosLocation in quad shader is 0

    Framebuffer::create(displayFb, screenspaceFb, bloomSearchFb, cocFb);
    Renderbuffer::create(rbo);

    Texture2D::create(colorTex, normalTex, ssrValues, positionTex, screenspaceTex, bloomTex, cocTex);
    ssrValues->setFormat(Texture::RG16F);
    cocTex->setFormat(Texture::Red16F);

    TextureCube::create(skybox);
    skybox->setFormat(Texture::RGB);
    skybox->bind();
    skybox->fromFile(resources "skybox/right.tga", TextureCube::Right);
    skybox->fromFile(resources "skybox/left.tga", TextureCube::Left);
    skybox->fromFile(resources "skybox/top.jpg", TextureCube::Top);
    skybox->fromFile(resources "skybox/bottom.tga", TextureCube::Bottom);
    skybox->fromFile(resources "skybox/front.tga", TextureCube::Front);
    skybox->fromFile(resources "skybox/back.tga", TextureCube::Back);
    skybox->setParams(TextureCube::defaultParams());
    skybox->unbind();

    Texture2D::setParamsMultiple(Texture2D::defaultParams(),
            colorTex, normalTex, ssrValues, positionTex, screenspaceTex, bloomTex, cocTex);

    TextureCreateInfo createInfo;
    createInfo.format = Texture::RGB16F;
    createInfo.width = winWidth * bloomK;
    createInfo.height = winHeight * bloomK;
    createInfo.params = Texture2D::defaultParams();

    bloomBlur = make_shared<Blur>(createInfo);
    bloomBlur->setPingPongShaders(bloomBlurHorShader, bloomBlurVertShader);
    bloomBlur->setQuadRenderer(quadRenderer.get());
    bloomBlur->configureKernel(bloomBlurKernelRadius, bloomBlurKernelSigma);

    createInfo.format = Texture::Red16F;
    createInfo.width = winWidth * dofK;
    createInfo.height = winHeight * dofK;

    cocBlur = make_shared<Blur>(createInfo);
    cocBlur->setPingPongShaders(cocBlurHorShader, cocBlurVertShader);
    cocBlur->setQuadRenderer(quadRenderer.get());
    cocBlur->configureKernel(cocBlurKernelRadius, cocBlurKernelSigma);

    createInfo.format = Texture::RGB16F;

    dofBlur = make_shared<Blur>(createInfo);
    dofBlur->setPingPongShaders(dofBlurHorShader, dofBlurVertShader);
    dofBlur->setQuadRenderer(quadRenderer.get());
    dofBlur->configureKernel(dofBlurKernelRadius, dofBlurKernelSigma);

    updateRenderTextures();

    rbo->bind();
    rbo->setWidthHeight(winWidth, winHeight);
    rbo->setFormat(Texture::DepthComponent);
    rbo->update();
    rbo->unbind();

    displayFb->bind();
    displayFb->attachRenderbuffer(rbo, Framebuffer::DepthAttachment);

    displayFb->addOutput(0, Framebuffer::ColorAttachmentZero + 0);
    displayFb->addOutput(1, Framebuffer::ColorAttachmentZero + 1);
    displayFb->addOutput(2, Framebuffer::ColorAttachmentZero + 2);
    displayFb->addOutput(3, Framebuffer::ColorAttachmentZero + 3);

    displayFb->addAttachmentsList();
    displayFb->setActiveAttachmentsList(1);
    displayFb->addOutput(0, Framebuffer::ColorAttachmentZero + 0);
    displayFb->addOutput(2, Framebuffer::ColorAttachmentZero + 2);
    // displayFb->update(); // not need here because it will be called each frame

    displayFb->attachTexture(colorTex, Framebuffer::ColorAttachmentZero + 0);
    displayFb->attachTexture(normalTex, Framebuffer::ColorAttachmentZero + 1);
    displayFb->attachTexture(positionTex, Framebuffer::ColorAttachmentZero + 2);
    displayFb->attachTexture(ssrValues, Framebuffer::ColorAttachmentZero + 3);

    screenspaceFb->bind();
    screenspaceFb->attachTexture(screenspaceTex, Framebuffer::ColorAttachmentZero);

    bloomSearchFb->bind();
    bloomSearchFb->attachTexture(bloomTex, Framebuffer::ColorAttachmentZero);

    cocFb->bind();
    cocFb->attachTexture(cocTex, Framebuffer::ColorAttachmentZero);
    cocFb->unbind();

    // configuring CS
    colorShader->bind();
    colorShader->setInt(Module::Material::Vars::AmbientTex, 0);
    colorShader->setInt(Module::Material::Vars::DiffuseTex, 1);
    colorShader->setInt(Module::Material::Vars::SpecularTex, 2);
    colorShader->setInt(Module::Material::Vars::NormalTex, 3);
    colorShader->setInt(Module::Material::Vars::ReflectionStrengthTex, 4);
    colorShader->setInt(Module::Material::Vars::JitterTex, 5);
    colorShader->setFloat(Module::Lighting::Vars::ShadowOpacity, shadowOpacity);

    // configuring CubemapShader
    skyboxShader->bind();
    skyboxShader->setVec3(CubemapShader::Vars::Color, glm::vec3(0.125f));
    skyboxShader->setFloat(CubemapShader::Vars::PosScaling, 64.0f);

    // blend setting
    blendShader->bind();
    blendShader->setInt(BlendShader::Vars::BaseImage, 0); // GL_TEXTURE0
    blendShader->setInt(BlendShader::Vars::BloomImage, 1); // GL_TEXTURE1
    blendShader->setInt(BlendShader::Vars::DofImage, 2); // GL_TEXTURE2
    blendShader->setInt(Module::BlendDOF::Vars::COCMap, 3); // GL_TEXTURE3
    blendShader->setFloat(Module::BlendDOF::Vars::DOFSigmaDivider, dofSigmaDivider);
    blendShader->setFloat(BlendShader::Vars::Exposure, blendExposure);
    blendShader->setFloat(BlendShader::Vars::Gamma, blendGamma);

    // screen space setting
    ssrShader->bind();
    ssrShader->setInt(SSRShader::Vars::NormalMap, 1);
    ssrShader->setInt(SSRShader::Vars::SSRValuesMap, 2);
    ssrShader->setInt(SSRShader::Vars::PositionMap, 3);
    ssrShader->unbind();
}

/**
 * Creating matrices
 */
void initCamera() {
    camera.setFar(64.0f);
    camera.setFieldOfView(glm::radians(60.0f));
    camera.setAspectRatio((float)winWidth / (float)winHeight);
    camera.perspective();
    
    camera.setPitch(glm::radians(30.0f));
    camera.rotate();

    camera.setPos(0, 10, 14);
    camera.translate();

    camera.updateMatrix();
    
    camController.camera = &camera;
}

/**
 * Creating shapes and loading textures
 */
void initShapes() {
    string path = resources "models/";

    createShapes(path + "chess/Classic Chess small.obj", path + "chess", 0, false, 0); // classic chess
    createShapes(path + "japanese_lamp/japanese_lamp.obj", path + "japanese_lamp", 1, true, 0); // Japanese lamp
    createShapes(path + "man/man.fbx", path + "man", 2, false, 4); // animated man
    createShapes(path + "astroboy/astroboy_walk.dae", path + "astroboy", 3, false, 4);
}

/**
 * Creating models from created buffers and loaded textures
 */
void createModels() {
    // classic chess
    models[0] = Model(Rotator::RotatorTypeSimple);
    models[0].setShape(shapes[0].get());

    // animated man
    manAnimator = Animator(shapes[2].get(), "Armature|Run");
    manAnimationBlender.setShape(shapes[2].get());
    manAnimationBlender.setFactor(0.25f);
    manAnimationBlender.setLhsAnim(0);
    manAnimationBlender.setRhsAnim(1);
    models[1] = Model(Rotator::RotatorTypeSimple);
    models[1].setShape(shapes[2].get());
    models[1].setX(-2.0f);
    models[1].translate();
    models[1].updateMatrix();
    models[1].setAnimator(&manAnimator);
    models[1].setBones(&manAnimationBlender.bones());

    // animated astroboy
    astroboyAnimator = Animator(shapes[3].get());
    models[2] = Model(Rotator::RotatorTypeSimple);
    models[2].setShape(shapes[3].get());
    models[2].setPitch(glm::radians(-90.0f));
    models[2].rotate();
    models[2].setScale(glm::vec3(50.0f));
    models[2].scale();
    models[2].setX(2.0f);
    models[2].translate();
    models[2].updateMatrix();
    models[2].setAnimator(&astroboyAnimator);
}

/**
 * Creating light sources
 */
void initLamps() {
    lamps[0] = Model(Rotator::RotatorTypeSimple);
    lamps[0].setShape(shapes[1].get());
    pointLamps[0].mptr = &lamps[0];
    createPointLamp(pointLamps[0], glm::vec3(0.0f, 8.0f, 15.0f), glm::vec3(1.0f, 1.0f, 1.0f), 0);
    lamps[0].translate();
    lamps[0].updateMatrix();

    lamps[1] = Model(Rotator::RotatorTypeSimple);
    lamps[1].setShape(shapes[1].get());
    dirLamps[0].mptr = &lamps[1];
    createDirLamp(dirLamps[0],
            glm::vec3(0.0f, 8.0f, -15.0f),
            glm::vec3(glm::radians(180.0f), glm::radians(30.0f), 0.0f),
            glm::vec3(253.0f / 255.0f, 184.0f / 255.0f, 19.0f / 255.0f), 0);
    lamps[1].translate();
    lamps[1].updateMatrix();
}

void initShadowMaps() {
    colorShader->bind();

    lightManager.configureShadowMapping();
    for (usize i = 0; i < pointLightsCount; i++)
        lightManager.transmitter.setShadowMap(pointLamps[i], i);
    for (usize i = 0; i < dirLightsCount; i++)
        lightManager.transmitter.setShadowMap(dirLamps[i], i);

    colorShader->unbind();
}

void initShadowCalculation() {
    colorShader->bind();
    colorShader->setFloat(Module::Lighting::Vars::ShadowDiskRadiusK, diskRadius_k);
    colorShader->setFloat(Module::Lighting::Vars::ShadowDiskRadiusMin, diskRadius_min);
    colorShader->unbind();
}

/**
 * Initialize Depth of field
 */
void initDOF() {
    dofCoCShader->bind();
    dofCoCShader->setFloat(COCShader::Vars::Cinematic::Aperture, dofAperture);
    dofCoCShader->setFloat(COCShader::Vars::Cinematic::ImageDistance, dofImageDistance);
    dofCoCShader->setFloat(COCShader::Vars::Cinematic::PlaneInFocus, -1.0f);
    dofCoCShader->unbind();
}

/* init code end */

/**
 * Cleans memory before exit
 */
void recycleAll() {
    Framebuffer::destroy(displayFb, screenspaceFb, bloomSearchFb, cocFb);

    Texture2D::destroy(colorTex, normalTex, ssrValues, positionTex, screenspaceTex, bloomTex, cocTex);
    TextureCube::destroy(skybox);

    Renderbuffer::destroy(rbo);

    Engine::destroy();
}

void sendLampsData() {
    lightManager.transmitter.setPointLightsCount(pointLightsCount);
    lightManager.transmitter.setDirLightsCount(dirLightsCount);
    colorShader->setVec3(ColorShader::Vars::CameraPos, camera.getPos());
    for (size_t i = 0; i < pointLightsCount; i++)
        lightManager.transmitter.setPos(pointLamps[i], i);
    for (size_t i = 0; i < dirLightsCount; i++)
        lightManager.transmitter.setPos(dirLamps[i], i);
}

#define value *

/* --- matrices --- */
glm::mat4 getMVPMatrix() {
    return camera.getProjectionMatrix() * camera.getViewMatrix() * value modelMatrix;
}

glm::mat4 getMVMatrix() {
    return camera.getViewMatrix() * value modelMatrix;
}

void updateMatrices() {
    colorShader->setMat4(ColorShader::Vars::MVPMatrix, getMVPMatrix());
    colorShader->setMat4(ColorShader::Vars::MVMatrix, getMVMatrix());
    colorShader->setMat4(ColorShader::Vars::ModelMatrix, value modelMatrix);
    colorShader->setMat4(ColorShader::Vars::ViewMatrix, camera.getViewMatrix());
}

/**
 * Draws model in depth map<br>
 * if point light, leave mat empty, but if dir light - it must be light space matrix
 */
void drawModelDM(const Model &model, ShaderProgram *program, const glm::mat4 &mat = glm::mat4(1.0f)) {
    Shape *shape = model.getShape();
    shape->inputLayouts[0]->bind();

    if (shape->bonesPerVertex != 0)
        for (int i = 0; i < shape->bonesStorage.count(); i++)
            ShaderProgram::setMat4(program->getLocation(BoneSystem::Vars::Bones) + i, model.getBone(i));

    program->setInt(BoneSystem::Vars::BoneAttribsPerVertex, (int)(shape->bonesPerVertex / 4 + (shape->bonesPerVertex % 4 == 0 ? 0 : 1)));
    program->setMat4(ShadowShader::Vars::TransformationMatrix, mat * model.m_transform);
    
    for (auto & mesh : shape->meshes)
        Engine::drawElements(mesh.start, mesh.count);
}

/**
 * Draws model
 */
inline void useNotNull(Texture2D *const tex, const uint slot) {
    if (tex != nullptr)
        tex->use(slot);
    else
        Engine::defaultTexture2D()->use(slot);
}

void drawModel(const Model &model) {
    Shape *shape = model.getShape();
    shape->inputLayouts[1]->bind();
    
    if (shape->bonesPerVertex != 0)
        for (int i = 0; i < shape->bonesStorage.count(); i++)
            ShaderProgram::setMat4(colorShader->getLocation(BoneSystem::Vars::Bones) + i, model.getBone(i));

    colorShader->setInt(BoneSystem::Vars::BoneAttribsPerVertex, shape->bonesPerVertex / 4 + (shape->bonesPerVertex % 4 == 0 ? 0 : 1));
    modelMatrix = &model.m_transform;
	updateMatrices();
    for (auto & mesh : shape->meshes) {
        Material &material = mesh.material;
        useNotNull(material.ambientTexture.get(), 0);
        useNotNull(material.diffuseTexture.get(), 1);
        useNotNull(material.specularTexture.get(), 2);
        useNotNull(material.normalTexture.get(), 3);
        useNotNull(material.reflectionTexture.get(), 4);
        useNotNull(material.jitterTexture.get(), 5);

        colorShader->setFloat(Module::Material::Vars::AmbientStrength, mesh.material.ambientStrength);
        colorShader->setFloat(Module::Material::Vars::DiffuseStrength, mesh.material.diffuseStrength);
        colorShader->setFloat(Module::Material::Vars::SpecularStrength, mesh.material.specularStrength);
        colorShader->setFloat(Module::Material::Vars::Shininess, mesh.material.shininess);

        Engine::drawElements(mesh.start, mesh.count);
    }
}

/**
 * Renders to depth cubemap
 */
void renderToDepthCubemap(const uint index) {
	pointLamps[index].begin();
    pointLamps[index].updateMatrix();
    pointLamps[index].shadowMapFb->clearDepthBuffer();

    lightManager.transmitter.setShadowShaderPos(pointLamps[index]);
	lightManager.transmitter.setShadowShaderMatrices(pointLamps[index]);

	// drawing models
    for (size_t i = 0; i < modelsCount; i++)
        drawModelDM(models[i], pointShadowShader);

	// drawing lamps
	for (GLuint i = 0; i < pointLightsCount; i++) {
		if (i == index) continue;
        drawModelDM(*pointLamps[i].mptr, pointShadowShader);
	}

	pointLamps[index].end();
}

/**
 * Renders to depth map
 */
void renderToDepthMap(uint index) {
	dirLamps[index].begin();
    dirLamps[index].shadowMapFb->clearDepthBuffer();

	// drawing models
    for (size_t i = 0; i < modelsCount; i++)
        drawModelDM(models[i], dirShadowShader, dirLamps[index].m_lightSpace);

	// drawing lamps
	for (GLuint i = 0; i < dirLightsCount; i++) {
		if (i == index) continue;
        drawModelDM(*dirLamps[i].mptr, dirShadowShader, dirLamps[index].m_lightSpace);
	}

	dirLamps[index].end();
}

/**
 * Color rendering
 */
void render() {
    displayFb->bind();
    displayFb->setActiveAttachmentsList(0);
    displayFb->update();
    displayFb->clear(Framebuffer::ColorBuffer | Framebuffer::DepthBuffer);

    Engine::setViewport(winWidth, winHeight);

    colorShader->bind();

    // sending lamps parameters to fragment shader
	sendLampsData();

    // drawing
    for (size_t i = 0; i < modelsCount; i++)
        drawModel(models[i]);
	for (size_t i = 0; i < pointLightsCount + dirLightsCount; i++)
	    drawModel(lamps[i]);

    // render skybox
    displayFb->setActiveAttachmentsList(1);
    displayFb->update();

    Engine::setDepthTestMode(Engine::DepthTestLessOrEqual);
    skyboxShader->bind();
    skyboxShader->setMat3(CubemapShader::Vars::ViewMatrix, glm::mat3(camera.getViewMatrix()));
    skyboxShader->setMat4(CubemapShader::Vars::TransformationMatrix, camera.getProjectionMatrix() * glm::mat4(glm::mat3(camera.getViewMatrix())));
    skybox->use(0);
    skyboxRenderer->getInputLayout()->bind();
    skyboxRenderer->draw();
    Engine::setDepthTestMode(Engine::DepthTestLess);

    // postprocessing
    quadRenderer->getInputLayout()->bind();

    screenspaceFb->bind();
    ssrShader->bind();
    colorTex->use(0);
    normalTex->use(1);
    ssrValues->use(2);
    positionTex->use(3);
    quadRenderer->draw();

    Engine::setViewport(winWidth * bloomK, winHeight * bloomK);
    bloomSearchFb->bind();
    bloomSearchShader->bind();
    screenspaceTex->use(0);
    quadRenderer->draw();
    bloomBlur->makeBlur(bloomTex);

    Engine::setViewport(winWidth * dofK, winHeight * dofK);
    cocFb->bind();
    dofCoCShader->bind();
    positionTex->use(0);
    quadRenderer->draw();

    cocBlur->makeBlur(cocTex);
    dofBlur->makeBlur(screenspaceTex);
    Engine::setViewport(winWidth, winHeight);

    Engine::defaultFramebuffer()->bind();
    Engine::defaultFramebuffer()->clearDepthBuffer(); // color buffer will cleared by quad rendering
    blendShader->bind();
    screenspaceTex->use(0);
    bloomBlur->get()->use(1);
    dofBlur->get()->use(2);
    cocBlur->get()->use(3);
    quadRenderer->draw();
    blendShader->unbind();
}

void display() {
    // animate
    for (usize i = 0; i < modelsCount; i++) {
        if (models[i].getShape()->bonesPerVertex != 0) {
            auto &animations = models[i].getShape()->animations;
            auto animator = models[i].getAnimator();

            for (uint j = 0; j < animations.size(); j++) {
                animator->setAnimationIndex(j);
                animator->animate(glfwGetTime());
            }
        }
    }

    manAnimationBlender.blend();

    // shadow rendering
    // point lights
    pointShadowShader->bind();
	for (uint i = 0; i < pointLightsCount; i++) {
	    lightManager.transmitter.setShadowShaderFarPlane(pointLamps[i]);
        renderToDepthCubemap(i);
    }

    // dir lights
    dirShadowShader->bind();
    for (uint i = 0; i < dirLightsCount; i++)
        renderToDepthMap(i);
	
    ssrShader->bind();
    ssrShader->setMat4(SSRShader::Vars::ProjectionMatrix, camera.getProjectionMatrix());
    ssrShader->setMat4(SSRShader::Vars::ViewMatrix, camera.getViewMatrix());

	/* --- color rendering --- */
	render();
}

void animate_scene() {
    glm::mat3 rotate = glm::mat3(glm::rotate(glm::mat4(), glm::radians(0.01f), glm::vec3(0, 1, 0)));
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        pointLamps[0].setPos(pointLamps[0].m_pos * rotate);
        pointLamps[0].translate();
        pointLamps[0].mptr->updateMatrix();
    }
}

// The MAIN function, from here we start the application and run the game loop
int main() {
    initGL();
    initShaders();
    initCamera();
    initShapes();
    createModels();
    initLamps();
    initShadowMaps();
    initShadowCalculation();
    initDOF();
    
    mouseEventListener.setCallback(mouse_callback);

    std::thread animate_scene_th(animate_scene);

    double currentTime, previousTime = glfwGetTime();
    size_t frameCount = 0;
    while (!glfwWindowShouldClose(window)) {
        currentTime = glfwGetTime();
        frameCount++;
        // If a second has passed.
        if (currentTime - previousTime >= 1.0) {
            // Display the average frame count and the average time for 1 frame
            std::cout << frameCount << " (" << (frameCount / (currentTime - previousTime)) << ") FPS, " << ((currentTime - previousTime) / frameCount) * 1000 << " ms\n";
            frameCount = 0;
            previousTime = currentTime;
        }

        display();
        // Check if any events have been activiated (key pressed, mouse moved etc.) and call corresponding response functions
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    animate_scene_th.detach();
    recycleAll();
    glfwTerminate(); // Terminate GLFW, clearing any resources allocated by GLFW.
    
    std::cout << "Exit with exit code 0\n";

    return 0;
}

#define keyPressed(keyCode) key == keyCode && action == GLFW_PRESS
#define keyHeld(keyCode) key == keyCode && (action == GLFW_PRESS || action == GLFW_REPEAT)

// Is called whenever a key is pressed/released via GLFW
void key_callback(GLFWwindow* glfwWindow, int key, int scancode, int action, int mode) {
    if (keyHeld(GLFW_KEY_1)) {
        manAnimationBlender.changeFactor(-0.025f);
    } else if (keyHeld(GLFW_KEY_2)) {
        manAnimationBlender.changeFactor(0.025f);
    } if (keyPressed(GLFW_KEY_M)) {
        Framebuffer *const dFramebuffer = dirLamps[0].shadowMapFb;
        dFramebuffer->bind();
        auto dPixelsData = dFramebuffer->getAllPixels2D(Framebuffer::DepthAttachment);
        TextureTools::saveImage(Path::join(Path::getWorkingDirectory(), "dir_depth.bmp"), dPixelsData, 3);
        dFramebuffer->unbind();

        Framebuffer *const pFramebuffer = pointLamps[0].shadowMapFb;
        pFramebuffer->bind();
        auto pPixelsData = pFramebuffer->getAllPixelsCube(TextureCube::Right, Framebuffer::DepthAttachment);
        TextureTools::saveImage(Path::join(Path::getWorkingDirectory(), "point_depth.bmp"), pPixelsData, 3);
        pFramebuffer->unbind();

        std::cout << "Depth map data saved\n";
    } else if (keyPressed(GLFW_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(glfwWindow, GL_TRUE);
    } else if (action == GLFW_REPEAT || action == GLFW_RELEASE) {
        if (key == GLFW_KEY_W || key == GLFW_KEY_S || key == GLFW_KEY_A || key == GLFW_KEY_D) {
            if (key == GLFW_KEY_W)
                camController.goForward();
            else if (key == GLFW_KEY_S)
                camController.goBack();
            else if (key == GLFW_KEY_A)
                camController.goLeft();
            else if (key == GLFW_KEY_D)
                camController.goRight();

            camera.translate();
            camera.updateMatrix();
        } else if (key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT || key == GLFW_KEY_UP || key == GLFW_KEY_DOWN) {
            glm::mat4 r;
            if (key == GLFW_KEY_LEFT)
                manHeadRotator.setYaw(manHeadRotator.getYaw() + glm::radians(5.0f));
            else if (key == GLFW_KEY_RIGHT)
                manHeadRotator.setYaw(manHeadRotator.getYaw() - glm::radians(5.0f));
            else if (key == GLFW_KEY_UP)
                manHeadRotator.setPitch(manHeadRotator.getPitch() + glm::radians(5.0f));
            else // GLFW_KEY_DOWN
                manHeadRotator.setPitch(manHeadRotator.getPitch() - glm::radians(5.0f));

            manHeadRotator.rotate(r);
            shapes[2]->setNodeTransform("Head", r);
        }
    }
}

void mouse_callback(GLFWwindow* glfwWindow, int button, int action, int mods) {
    float x, y;

    {
        double d_x, d_y;
        glfwGetCursorPos(glfwWindow, &d_x, &d_y);
        x = static_cast<float>(d_x);
        y = static_cast<float>(d_y);
    }
    
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        mouseEventListener.buttonDown(x, y, MouseEventListener::ButtonRight);
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
        mouseEventListener.buttonUp(x, y, MouseEventListener::ButtonRight);
    
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        mouseEventListener.buttonDown(x, y, MouseEventListener::ButtonLeft);
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        mouseEventListener.buttonUp(x, y, MouseEventListener::ButtonLeft);
    }
}

void cursor_pos_callback(GLFWwindow* glfwWindow, double x, double y) {
    mouseEventListener.mouseMove(x, y);
}

void mouse_callback(MouseEvent *event) {
    switch(event->action) {
        case MouseEventListener::ActionDown:
            camController.setMousePos(event->getX(), event->getY());
            break;
        case MouseEventListener::ActionMove:
            if (!event->listener->getLeftButton().isPressed)
                break;
            
            camController.mouseMove(event->getX(), event->getY());
            camera.rotate();
            camera.updateMatrix();
            break;
        case MouseEventListener::ActionLongClick:
            cout << "Long click " << event->button << "\n";
            break;
        case MouseEventListener::ActionClick:
            uint x = static_cast<uint>(event->getX());
            uint y = static_cast<uint>(event->getY());

            displayFb->bind();
            auto pixels = displayFb->getPixels2D(Framebuffer::ColorAttachmentZero + 2, x, winHeight - y, 1, 1).pixels;
            displayFb->unbind();

            cout << "Click: x: " << x << "; y: " << y << "\n";
            cout << "Position map: x: " << pixels[0] << "; y: " << pixels[1] << "; z: " << pixels[2] << "\n";
        
            dofCoCShader->bind();
            dofCoCShader->setFloat(COCShader::Vars::Cinematic::PlaneInFocus, pixels[2] == 0 ? FLT_EPSILON : pixels[2]);
            dofCoCShader->unbind();
            break;
    }
}
