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
#include <algine/core/PtrMaker.h>
#include <algine/core/shader/ShaderProgram.h>
#include <algine/core/shader/ShaderProgramManager.h>
#include <algine/core/Framebuffer.h>
#include <algine/core/Renderbuffer.h>
#include <algine/core/texture/Texture2D.h>
#include <algine/core/texture/TextureCube.h>
#include <algine/core/texture/TextureCubeManager.h>
#include <algine/core/texture/TextureTools.h>

#include <algine/std/camera/Camera.h>
#include <algine/std/camera/FPSCameraController.h>
#include <algine/std/rotator/EulerRotator.h>
#include <algine/std/model/Model.h>
#include <algine/std/model/ModelManager.h>
#include <algine/std/lighting/DirLamp.h>
#include <algine/std/lighting/PointLamp.h>
#include <algine/std/lighting/LightingManager.h>
#include <algine/std/CubeRendererPtr.h>
#include <algine/std/CubeRenderer.h>
#include <algine/std/QuadRenderer.h>
#include <algine/std/animation/AnimationBlender.h>
#include <algine/std/animation/BoneSystemManager.h>

#include <algine/ext/debug.h>
#include <algine/ext/Blur.h>
#include <algine/ext/event/MouseEvent.h>
#include <algine/ext/constants/SSRShader.h>
#include <algine/ext/constants/COCShader.h>
#include <algine/ext/constants/BlendDOF.h>

#include <algine/constants/BoneSystem.h>
#include <algine/constants/ShadowShader.h>
#include <algine/constants/CubemapShader.h>
#include <algine/constants/Material.h>

#include "ColorShader.h"
#include "BlendShader.h"
#include "constants.h"

using namespace algine;
using namespace std;
using namespace tulz;

constexpr bool fullscreen = false;

constexpr uint shapesCount = 4;
constexpr uint modelsCount = 3;

// Function prototypes
void key_callback(GLFWwindow* glfwWindow, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* glfwWindow, int button, int action, int mods);
void mouse_callback(MouseEvent *event);
void cursor_pos_callback(GLFWwindow* glfwWindow, double x, double y);

// Window dimensions
uint winWidth = 1366, winHeight = 763;
GLFWwindow* window;

// models
ShapePtr shapes[shapesCount];
ModelPtr models[modelsCount], lamps[pointLightsCount + dirLightsCount];
AnimationBlender manAnimationBlender;
BoneSystemManager boneManager;

// light
PointLamp pointLamps[pointLightsCount];
DirLamp dirLamps[dirLightsCount];
LightingManager lightManager;

CubeRendererPtr skyboxRenderer;
QuadRendererPtr quadRenderer;

Ptr<Blur> bloomBlur;
Ptr<Blur> dofBlur;
Ptr<Blur> cocBlur;

FramebufferPtr displayFb;
FramebufferPtr screenspaceFb;
FramebufferPtr bloomSearchFb;
FramebufferPtr cocFb;

Texture2DPtr colorTex;
Texture2DPtr normalTex;
Texture2DPtr ssrValues;
Texture2DPtr positionTex;
Texture2DPtr screenspaceTex;
Texture2DPtr bloomTex;
Texture2DPtr cocTex;
TextureCubePtr skybox;

ShaderProgramPtr skyboxShader;
ShaderProgramPtr colorShader;
ShaderProgramPtr pointShadowShader;
ShaderProgramPtr dirShadowShader;
ShaderProgramPtr dofBlurHorShader, dofBlurVertShader;
ShaderProgramPtr dofCoCShader;
ShaderProgramPtr ssrShader;
ShaderProgramPtr bloomSearchShader;
ShaderProgramPtr bloomBlurHorShader, bloomBlurVertShader;
ShaderProgramPtr cocBlurHorShader, cocBlurVertShader;
ShaderProgramPtr blendShader;

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

void resize() {
    displayFb->resizeAttachments(winWidth, winHeight);
    screenspaceFb->resizeAttachments(winWidth, winHeight); // TODO: make small sst + blend pass in the future
    bloomSearchFb->resizeAttachments(winWidth * bloomK, winHeight * bloomK);
    cocFb->resizeAttachments(winWidth * dofK, winHeight * dofK);

    bloomBlur->resizeOutput(winWidth * bloomK, winHeight * bloomK);
    cocBlur->resizeOutput(winWidth * dofK, winHeight * dofK);
    dofBlur->resizeOutput(winWidth * dofK, winHeight * dofK);
}

void window_size_callback(GLFWwindow* window, int width, int height) {
    winWidth = width;
    winHeight = height;

    resize();

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

    lightManager.bindBuffer();
    lightManager.writeColor(result, id);
    lightManager.writeKc(result, id);
    lightManager.writeKl(result, id);
    lightManager.writeKq(result, id);
    lightManager.writeFarPlane(result, id);
    lightManager.writeBias(result, id);
    lightManager.unbindBuffer();
}

void createDirLamp(DirLamp &result,
        const glm::vec3 &pos, const glm::vec3 &rotate,
        const glm::vec3 &color,
        usize id)
{
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

    lightManager.bindBuffer();
    lightManager.writeColor(result, id);
    lightManager.writeKc(result, id);
    lightManager.writeKl(result, id);
    lightManager.writeKq(result, id);
    lightManager.writeMinBias(result, id);
    lightManager.writeMaxBias(result, id);
    lightManager.writeLightMatrix(result, id);
    lightManager.unbindBuffer();
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
    cout << "Starting GLFW context, OpenGL 3.3" << endl;

    // Init GLFW
    if (!glfwInit())
        cout << "GLFW init failed";

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
        cout << "GLEW init failed\n";

#ifdef DEBUG_OUTPUT
    enableGLDebugOutput();
#endif

    cout << "Your GPU vendor: " << Engine::getGPUVendor() << "\nYour GPU renderer: " << Engine::getGPURenderer() << "\n";

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
    lightManager.setLightsLimit(dirLightsLimit, Light::Type::Dir);
    lightManager.setLightsLimit(pointLightsLimit, Light::Type::Point);
    lightManager.setLightsMapInitialSlot(6, Light::Type::Point);
    lightManager.setLightsMapInitialSlot(
            lightManager.getLightsMapInitialSlot(Light::Type::Point) + lightManager.getLightsLimit(Light::Type::Point),
            Light::Type::Dir);

    cout << "Compiling algine shaders\n";

    ShaderManager::setGlobalIncludePaths({
        algineResources,
        resources "shaders"
    });

    auto programFromConfig = [](ShaderProgramPtr &program, const string &configName) {
        ShaderProgramManager manager;
        manager.importFromFile(resources "programs/" + configName + ".conf.json");
        program = manager.create();
        program->loadActiveLocations();
    };

    programFromConfig(colorShader, "Color");
    programFromConfig(pointShadowShader, "PointShadow");
    programFromConfig(dirShadowShader, "DirShadow");
    programFromConfig(dofCoCShader, "DofCoc");
    programFromConfig(blendShader, "Blend");
    programFromConfig(bloomBlurHorShader, "BloomBlur.hor");
    programFromConfig(bloomBlurVertShader, "BloomBlur.ver");
    programFromConfig(dofBlurHorShader, "DofBlur.hor");
    programFromConfig(dofBlurVertShader, "DofBlur.ver");
    programFromConfig(cocBlurHorShader, "CocBlur.hor");
    programFromConfig(cocBlurVertShader, "CocBlur.ver");
    programFromConfig(skyboxShader, "Skybox");
    programFromConfig(ssrShader, "SSR");
    programFromConfig(bloomSearchShader, "BloomSearch");

    cout << "Compilation done\n";

    // TODO: shaders get()
    lightManager.setLightShader(colorShader.get());
    lightManager.setPointLightShadowShader(pointShadowShader.get());
    lightManager.setBindingPoint(1);
    lightManager.init();

    lightManager.bindBuffer();
    lightManager.writePointLightsCount(pointLightsCount);
    lightManager.writeDirLightsCount(dirLightsCount);
    lightManager.writeShadowOpacity(shadowOpacity);
    lightManager.writeShadowDiskRadiusK(diskRadius_k);
    lightManager.writeShadowDiskRadiusMin(diskRadius_min);
    lightManager.unbindBuffer();

    skyboxRenderer = PtrMaker::make(skyboxShader->getLocation(CubemapShader::Vars::InPos));
    quadRenderer = PtrMaker::make(0); // inPosLocation in quad shader is 0

    PtrMaker::create(
        displayFb, screenspaceFb, bloomSearchFb, cocFb,
        colorTex, normalTex, ssrValues, positionTex, screenspaceTex, bloomTex, cocTex
    );

    ssrValues->setFormat(Texture::RG16F);
    cocTex->setFormat(Texture::Red16F);

    TextureCubeManager skyboxManager;
    skyboxManager.importFromFile(resources "textures/skybox/Skybox.conf.json");
    skybox = skyboxManager.create();

    Texture2D::setParamsMultiple(Texture2D::defaultParams(),
            colorTex.get(), normalTex.get(), ssrValues.get(), positionTex.get(),
            screenspaceTex.get(), bloomTex.get(), cocTex.get());

    TextureCreateInfo createInfo;
    createInfo.format = Texture::RGB16F;
    createInfo.width = winWidth * bloomK;
    createInfo.height = winHeight * bloomK;
    createInfo.params = Texture2D::defaultParams();

    bloomBlur = PtrMaker::make<Blur>(createInfo);
    bloomBlur->setPingPongShaders(bloomBlurHorShader, bloomBlurVertShader);
    bloomBlur->setQuadRenderer(quadRenderer);
    bloomBlur->configureKernel(bloomBlurKernelRadius, bloomBlurKernelSigma);

    createInfo.format = Texture::Red16F;
    createInfo.width = winWidth * dofK;
    createInfo.height = winHeight * dofK;

    cocBlur = PtrMaker::make<Blur>(createInfo);
    cocBlur->setPingPongShaders(cocBlurHorShader, cocBlurVertShader);
    cocBlur->setQuadRenderer(quadRenderer);
    cocBlur->configureKernel(cocBlurKernelRadius, cocBlurKernelSigma);

    createInfo.format = Texture::RGB16F;

    dofBlur = PtrMaker::make<Blur>(createInfo);
    dofBlur->setPingPongShaders(dofBlurHorShader, dofBlurVertShader);
    dofBlur->setQuadRenderer(quadRenderer);
    dofBlur->configureKernel(dofBlurKernelRadius, dofBlurKernelSigma);

    RenderbufferPtr rbo = PtrMaker::make();
    rbo->bind();
    rbo->setDimensions(winWidth, winHeight);
    rbo->setFormat(Texture::DepthComponent);
    rbo->update();
    rbo->unbind();

    displayFb->bind();
    displayFb->attachRenderbuffer(rbo, Framebuffer::DepthAttachment);
    displayFb->addOutputList(); // 2-nd list

    {
        auto &list = displayFb->getOutputList(0);
        list.addColor(0);
        list.addColor(1);
        list.addColor(2);
        list.addColor(3);
    }

    {
        auto &list = displayFb->getOutputList(1);
        list.addColor(0);
        list.addColor(2);
    }

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

    resize();
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

// Creating shapes and loading textures
#define modelsPath resources "models/"

/**
 * Creating models from created buffers and loaded textures
 */
void createModels() {
    auto getModel = [](const string &path)
    {
        ModelManager modelManager;
        modelManager.importFromFile(modelsPath + path);

        return modelManager.get();
    };

    models[0] = getModel("chess/Classic Chess small.json");
    models[1] = getModel("man/man.json");
    models[2] = getModel("astroboy/astroboy_walk.json");

    // animated man
    manAnimationBlender.setShape(models[1]->getShape());
    manAnimationBlender.setFactor(0.25f);
    manAnimationBlender.setLhsAnim(0);
    manAnimationBlender.setRhsAnim(1);

    models[1]->setBones(&manAnimationBlender.bones());

    boneManager.setBindingPoint(0);
    boneManager.setShaderPrograms({colorShader.get(), dirShadowShader.get(), pointShadowShader.get()}); // TODO: get()
    boneManager.setMaxModelsCount(2);
    boneManager.init();
    boneManager.getBlockBufferStorage().bind();
    boneManager.addModels({models[1].get(), models[2].get()}); // TODO: get()
}

/**
 * Creating light sources
 */
void initLamps() {
    ShapeManager manager;
    manager.importFromFile(modelsPath "japanese_lamp/japanese_lamp.shape.json");

    auto lampShape = manager.get();

    lamps[0] = PtrMaker::make();
    lamps[0]->setShape(lampShape);
    pointLamps[0].mptr = lamps[0].get(); // TODO: get()
    createPointLamp(pointLamps[0], {0.0f, 8.0f, 15.0f}, {1.0f, 1.0f, 1.0f}, 0);
    lamps[0]->translate();
    lamps[0]->transform();

    lamps[1] = PtrMaker::make();
    lamps[1]->setShape(lampShape);
    dirLamps[0].mptr = lamps[1].get();
    createDirLamp(dirLamps[0],
            {0.0f, 8.0f, -15.0f},
            {glm::radians(180.0f), glm::radians(30.0f), 0.0f},
            {253.0f / 255.0f, 184.0f / 255.0f, 19.0f / 255.0f}, 0);
    lamps[1]->translate();
    lamps[1]->transform();
}

void initShadowMaps() {
    colorShader->bind();

    lightManager.configureShadowMapping();

    for (usize i = 0; i < pointLightsCount; i++)
        lightManager.pushShadowMap(pointLamps[i], i);
    for (usize i = 0; i < dirLightsCount; i++)
        lightManager.pushShadowMap(dirLamps[i], i);

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
    Engine::destroy();
}

void sendLampsData() {
    lightManager.bindBuffer();
    colorShader->setVec3(ColorShader::Vars::CameraPos, camera.getPos());

    for (size_t i = 0; i < pointLightsCount; i++)
        lightManager.writePos(pointLamps[i], i);

    for (size_t i = 0; i < dirLightsCount; i++)
        lightManager.writePos(dirLamps[i], i);

    lightManager.unbindBuffer();
}

/* --- matrices --- */
glm::mat4 getMVPMatrix(const glm::mat4 &modelMatrix) {
    return camera.getProjectionMatrix() * camera.getViewMatrix() * modelMatrix;
}

glm::mat4 getMVMatrix(const glm::mat4 &modelMatrix) {
    return camera.getViewMatrix() * modelMatrix;
}

void updateMatrices(const glm::mat4 &modelMatrix) {
    colorShader->setMat4(ColorShader::Vars::MVPMatrix, getMVPMatrix(modelMatrix));
    colorShader->setMat4(ColorShader::Vars::MVMatrix, getMVMatrix(modelMatrix));
    colorShader->setMat4(ColorShader::Vars::ModelMatrix, modelMatrix);
    colorShader->setMat4(ColorShader::Vars::ViewMatrix, camera.getViewMatrix());
}

/**
 * Draws model in depth map<br>
 * if point light, leave mat empty, but if dir light - it must be light space matrix
 */
void drawModelDM(Model &model, ShaderProgramPtr &program, const glm::mat4 &mat = glm::mat4(1.0f)) {
    auto &shape = model.getShape();
    shape->inputLayouts[0]->bind();

    boneManager.linkBuffer(&model);

    program->setMat4(ShadowShader::Vars::TransformationMatrix, mat * model.m_transform);
    
    for (auto & mesh : shape->meshes) {
        Engine::drawElements(mesh.start, mesh.count);
    }
}

/**
 * Draws model
 */
void drawModel(Model &model) {
    auto &shape = model.getShape();
    shape->inputLayouts[1]->bind();

    boneManager.linkBuffer(&model);

	updateMatrices(model.transformation());

    for (auto & mesh : shape->meshes) {
        using namespace Module::Material::Vars;

        auto useNotNull = [](const Texture2DPtr &tex, const uint slot)
        {
            if (tex != nullptr) {
                tex->use(slot);
            } else {
                Engine::defaultTexture2D()->use(slot);
            }
        };

        Material &material = mesh.material;
        useNotNull(material.ambientTexture, 0);
        useNotNull(material.diffuseTexture, 1);
        useNotNull(material.specularTexture, 2);
        useNotNull(material.normalTexture, 3);
        useNotNull(material.reflectionTexture, 4);
        useNotNull(material.jitterTexture, 5);

        colorShader->setFloat(AmbientStrength, mesh.material.ambientStrength);
        colorShader->setFloat(DiffuseStrength, mesh.material.diffuseStrength);
        colorShader->setFloat(SpecularStrength, mesh.material.specularStrength);
        colorShader->setFloat(Shininess, mesh.material.shininess);

        Engine::drawElements(mesh.start, mesh.count);
    }
}

/**
 * Renders to depth cubemap
 */
void renderToDepthCubemap(const uint index) {
	pointLamps[index].begin();
    pointLamps[index].updateMatrix();
    pointLamps[index].getShadowFramebuffer()->clearDepthBuffer();

    lightManager.pushShadowShaderPos(pointLamps[index]);
	lightManager.pushShadowShaderMatrices(pointLamps[index]);

	// drawing models
    for (size_t i = 0; i < modelsCount; i++)
        drawModelDM(*models[i].get(), pointShadowShader);

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
    dirLamps[index].getShadowFramebuffer()->clearDepthBuffer();

	// drawing models
    for (uint i = 0; i < modelsCount; i++)
        drawModelDM(*models[i].get(), dirShadowShader, dirLamps[index].getLightSpaceMatrix());

	// drawing lamps
	for (uint i = 0; i < dirLightsCount; i++) {
		if (i == index)
		    continue;

        drawModelDM(*dirLamps[i].mptr, dirShadowShader, dirLamps[index].getLightSpaceMatrix());
	}

	dirLamps[index].end();
}

/**
 * Color rendering
 */
void render() {
    displayFb->bind();
    displayFb->setActiveOutputList(0);
    displayFb->update();
    displayFb->clear(Framebuffer::ColorBuffer | Framebuffer::DepthBuffer);

    Engine::setViewport(winWidth, winHeight);

    colorShader->bind();

    // sending lamps parameters to fragment shader
	sendLampsData();

    // drawing
    for (size_t i = 0; i < modelsCount; i++)
        drawModel(*models[i].get());

	for (size_t i = 0; i < pointLightsCount + dirLightsCount; i++)
	    drawModel(*lamps[i].get());

    // render skybox
    displayFb->setActiveOutputList(1);
    displayFb->update();

    // TODO: Engine::DepthTest::LessOrEqual
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
    bloomBlur->makeBlur(bloomTex.get());

    Engine::setViewport(winWidth * dofK, winHeight * dofK);
    cocFb->bind();
    dofCoCShader->bind();
    positionTex->use(0);
    quadRenderer->draw();

    cocBlur->makeBlur(cocTex.get());
    dofBlur->makeBlur(screenspaceTex.get());
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
        if (models[i]->getShape()->isBonesPresent()) {
            auto &animations = models[i]->getShape()->animations;
            auto animator = models[i]->getAnimator();

            for (uint j = 0; j < animations.size(); j++) {
                animator->setAnimationIndex(j);
                animator->animate(glfwGetTime());
            }
        }
    }

    manAnimationBlender.blend();

    boneManager.getBlockBufferStorage().bind();
    boneManager.writeBonesForAll();
    Engine::defaultUniformBuffer()->bind();

    // shadow rendering
    // point lights
    pointShadowShader->bind();
	for (uint i = 0; i < pointLightsCount; i++) {
	    lightManager.pushShadowShaderFarPlane(pointLamps[i]);
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
        this_thread::sleep_for(chrono::milliseconds(1));
        pointLamps[0].setPos(pointLamps[0].m_pos * rotate);
        pointLamps[0].translate();
        pointLamps[0].mptr->transform();
    }
}

// The MAIN function, from here we start the application and run the game loop
int main() {
    initGL();
    initShaders();
    initCamera();
    createModels();
    initLamps();
    initShadowMaps();
    initDOF();
    
    mouseEventListener.setCallback(mouse_callback);

    thread animate_scene_th(animate_scene);

    double currentTime, previousTime = glfwGetTime();
    size_t frameCount = 0, frames = 0, passes = 0;

    while (!glfwWindowShouldClose(window)) {
        currentTime = glfwGetTime();
        frameCount++;
        // If a second has passed.
        if (currentTime - previousTime >= 1.0) {
            // Display the average frame count and the average time for 1 frame
            cout << frameCount << " (" << (frameCount / (currentTime - previousTime)) << ") FPS, " << ((currentTime - previousTime) / frameCount) * 1000 << " ms\n";
            frames += frameCount;
            passes++;
            frameCount = 0;
            previousTime = currentTime;
        }

        display();
        // Check if any events have been activiated (key pressed, mouse moved etc.) and call corresponding response functions
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    cout << "Average: " << frames / (float) passes << " fps; " << (passes / (float) frames) * 1000.0f <<  " ms\n";

    animate_scene_th.detach();
    recycleAll();
    glfwTerminate(); // Terminate GLFW, clearing any resources allocated by GLFW.
    
    cout << "Exit with exit code 0\n";

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
        auto dFramebuffer = dirLamps[0].getShadowFramebuffer();
        dFramebuffer->bind();
        auto dPixelsData = dFramebuffer->getAllPixels2D(Framebuffer::DepthAttachment);
        TextureTools::save(Path::join(Path::getWorkingDirectory(), "dir_depth.bmp"), dPixelsData, 3);
        dFramebuffer->unbind();

        auto pFramebuffer = pointLamps[0].getShadowFramebuffer();
        pFramebuffer->bind();
        auto pPixelsData = pFramebuffer->getAllPixelsCube(TextureCube::Face::Right, Framebuffer::DepthAttachment);
        TextureTools::save(Path::join(Path::getWorkingDirectory(), "point_depth.bmp"), pPixelsData, 3);
        pFramebuffer->unbind();

        cout << "Depth map data saved\n";
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
            models[1]->getShape()->setBoneTransform("Head", r);
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
