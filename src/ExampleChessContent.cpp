#define GLM_FORCE_CTOR_INIT

#include "ExampleChessContent.h"

#include "constants.h" // TODO move dof
#include "BlendShader.h"
#include "ColorShader.h"

#include <algine/core/Engine.h>
#include <algine/core/window/Window.h>
#include <algine/core/shader/ShaderCreator.h>
#include <algine/core/shader/ShaderProgramCreator.h>
#include <algine/core/texture/TextureCubeCreator.h>
#include <algine/core/PtrMaker.h>

#include <algine/std/model/Shape.h>
#include <algine/std/model/Model.h>
#include <algine/std/model/ModelCreator.h>
#include <algine/std/CubeRenderer.h>

#include <algine/ext/constants/SSRShader.h>
#include <algine/ext/constants/BlendDOF.h>
#include <algine/ext/constants/COCShader.h>

#include <algine/constants/CubemapShader.h>
#include <algine/constants/ShadowShader.h>
#include <algine/constants/Material.h>

#include <iostream>
#include <cfloat>

using namespace std;

#define constant constexpr static auto

constant blendExposure = 6.0f, blendGamma = 1.125f;

// DOF variables
constant dofImageDistance = 1.0f;
constant dofAperture = 10.0f;
constant dofSigmaDivider = 1.5f;

// diskRadius variables
constant diskRadius_k = 1.0f / 25.0f;
constant diskRadius_min = 0.0f;

// shadow opacity: 1.0 - opaque shadow (by default), 0.0 - transparent
constant shadowOpacity = 0.65f;

ExampleChessContent::~ExampleChessContent() {
    lampThread.stopLoop();
}

void ExampleChessContent::init() {
    initShaders();
    initCamera();
    createModels();
    initLamps();
    initShadowMaps();
    initDOF();

    Engine::enableDepthTest();
    Engine::enableDepthMask();
    Engine::enableFaceCulling();

    getWindow()->setEventHandler(this);

    lampThread.content = this;
    lampThread.startLoop();
}

void ExampleChessContent::render() {
    pollKeys();

    // animate
    for (auto &model : models) {
        if (model->getShape()->isBonesPresent()) {
            auto animationsAmount = model->getShape()->getAnimationsAmount();
            auto animator = model->getAnimator();

            for (uint j = 0; j < animationsAmount; j++) {
                animator->setAnimationIndex(j);
                animator->animate((float)(Engine::timeFromStart()) / 1000.0f);
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

    for (uint i = 0; i < pointLamps.size(); i++) {
        lightManager.pushShadowShaderFarPlane(pointLamps[i]);
        renderToDepthCubemap(i);
    }

    // dir lights
    dirShadowShader->bind();

    for (uint i = 0; i < dirLamps.size(); i++)
        renderToDepthMap(i);

    ssrShader->bind();
    ssrShader->setMat4(SSRShader::Vars::ProjectionMatrix, camera.getProjectionMatrix());
    ssrShader->setMat4(SSRShader::Vars::ViewMatrix, camera.getViewMatrix());

    /* --- color rendering --- */
    renderScene();
}

void ExampleChessContent::mouseMove(double x, double y) {
    glm::vec2 mousePos = {x, y};

    constant k = 0.0025f;

    camera.changeRotation(glm::vec3(mousePos.y - lastMousePos.y, mousePos.x - lastMousePos.x, 0) * k);
    camera.rotate();
    camera.updateMatrix();

    lastMousePos = mousePos;
}

void ExampleChessContent::mouseClick(MouseKey key) {
    if (key != MouseKey::Left)
        return;

    displayFb->bind();
    auto pixels = displayFb->getPixels2D(Framebuffer::ColorAttachmentZero + 2, width() / 2, height() / 2, 1, 1).pixels;
    displayFb->unbind();

    cout << "Position map: x: " << pixels[0] << "; y: " << pixels[1] << "; z: " << pixels[2] << "\n";

    dofCoCShader->bind();
    dofCoCShader->setFloat(COCShader::Vars::Cinematic::PlaneInFocus, pixels[2] == 0 ? FLT_EPSILON : pixels[2]);
    dofCoCShader->unbind();
}

void ExampleChessContent::keyboardKeyPress(KeyboardKey key) {
    if (key == KeyboardKey::F) {
        getWindow()->setFullscreen(!getWindow()->isFullscreen());
    }
}

void ExampleChessContent::windowSizeChange(int, int) {
    resize();

    camera.setAspectRatio((float) width() / height());
    camera.perspective();
}

void ExampleChessContent::resize() {
    displayFb->resizeAttachments(width(), height());
    screenspaceFb->resizeAttachments(width(), height()); // TODO: make small sst + blend pass in the future
    bloomSearchFb->resizeAttachments(width() * bloomK, height() * bloomK);
    cocFb->resizeAttachments(width() * dofK, height() * dofK);

    bloomBlur->resizeOutput(width() * bloomK, height() * bloomK);
    cocBlur->resizeOutput(width() * dofK, height() * dofK);
    dofBlur->resizeOutput(width() * dofK, height() * dofK);
}

void ExampleChessContent::pollKeys() {
    auto isKeyPressed = [&](KeyboardKey key) {
        return getWindow()->isKeyPressed(key);
    };

    if (isKeyPressed(KeyboardKey::Escape))
        getWindow()->close();

    constant cameraMoveK = 0.5f;

    if (isKeyPressed(KeyboardKey::W))
        camera.goForward(cameraMoveK);

    if (isKeyPressed(KeyboardKey::A))
        camera.goLeft(cameraMoveK);

    if (isKeyPressed(KeyboardKey::S))
        camera.goBack(cameraMoveK);

    if (isKeyPressed(KeyboardKey::D))
        camera.goRight(cameraMoveK);

    auto rotateManHead = [&](const glm::vec3 &dRotate) {
        glm::mat4 r(1.0f);

        manHeadRotator.changeRotation(dRotate);
        manHeadRotator.rotate(r);

        models[1]->setBoneTransform("Head", r);
    };

    if (isKeyPressed(KeyboardKey::Up))
        rotateManHead({glm::radians(5.0f), 0.0f, 0.0f});

    if (isKeyPressed(KeyboardKey::Left))
        rotateManHead({0.0f, glm::radians(5.0f), 0.0f});

    if (isKeyPressed(KeyboardKey::Down))
        rotateManHead({-glm::radians(5.0f), 0.0f, 0.0f});

    if (isKeyPressed(KeyboardKey::Right))
        rotateManHead({0.0f, -glm::radians(5.0f), 0.0f});

    if (isKeyPressed(KeyboardKey::Key1)) {
        manAnimationBlender.changeFactor(-0.025f);
    } else if (isKeyPressed(KeyboardKey::Key2)) {
        manAnimationBlender.changeFactor(0.025f);
    }
}

void ExampleChessContent::createPointLamp(PointLamp &result, const glm::vec3 &pos, const glm::vec3 &color, usize id) {
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

void ExampleChessContent::createDirLamp(DirLamp &result, const glm::vec3 &pos, const glm::vec3 &rotate,
                                        const glm::vec3 &color, usize id) {
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

void ExampleChessContent::initShaders() {
    cout << "Compiling algine shaders\n";

    ShaderCreator::setGlobalIncludePaths({
        algineResources,
        resources "shaders"
    });

    auto programFromConfig = [](ShaderProgramPtr &program, const string &configName) {
        ShaderProgramCreator creator;
        creator.importFromFile(resources "programs/" + configName + ".conf.json");
        program = creator.create();
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

    skyboxRenderer = PtrMaker::make(skyboxShader->getLocation(CubemapShader::Vars::InPos));
    quadRenderer = PtrMaker::make(0); // inPosLocation in quad shader is 0

    PtrMaker::create(
        displayFb, screenspaceFb, bloomSearchFb, cocFb,
        colorTex, normalTex, ssrValues, positionTex, screenspaceTex, bloomTex, cocTex
    );

    ssrValues->setFormat(Texture::RG16F);
    cocTex->setFormat(Texture::Red16F);

    TextureCubeCreator skyboxCreator;
    skyboxCreator.importFromFile(resources "textures/skybox/Skybox.conf.json");
    skybox = skyboxCreator.create();

    Texture2D::setParamsMultiple(Texture2D::defaultParams(),
            colorTex.get(), normalTex.get(), ssrValues.get(), positionTex.get(),
            screenspaceTex.get(), bloomTex.get(), cocTex.get());

    TextureCreateInfo createInfo;
    createInfo.format = Texture::RGB16F;
    createInfo.width = width() * bloomK;
    createInfo.height = height() * bloomK;
    createInfo.params = Texture2D::defaultParams();

    bloomBlur = PtrMaker::make<Blur>(createInfo);
    bloomBlur->setPingPongShaders(bloomBlurHorShader, bloomBlurVertShader);
    bloomBlur->setQuadRenderer(quadRenderer);
    bloomBlur->configureKernel(bloomBlurKernelRadius, bloomBlurKernelSigma);

    createInfo.format = Texture::Red16F;
    createInfo.width = width() * dofK;
    createInfo.height = height() * dofK;

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
    rbo->setDimensions(width(), height());
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

void ExampleChessContent::initCamera() {
    camera.setFar(64.0f);
    camera.setFieldOfView(glm::radians(60.0f));
    camera.setAspectRatio((float) width() / (float) height());
    camera.perspective();

    camera.setPitch(glm::radians(30.0f));
    camera.rotate();

    camera.setPos(0, 10, 14);
    camera.translate();

    camera.updateMatrix();
}

#define modelsPath resources "models/"

void ExampleChessContent::createModels() {
    auto getModel = [](const string &path)
    {
        ModelCreator modelCreator;
        modelCreator.importFromFile(modelsPath + path);

        return modelCreator.get();
    };

    models.emplace_back(getModel("chess/Classic Chess small.json"));
    models.emplace_back(getModel("man/man.json"));
    models.emplace_back(getModel("astroboy/astroboy_walk.json"));

    // animated man
    manAnimationBlender.setModel(models[1]);
    manAnimationBlender.setFactor(0.25f);
    manAnimationBlender.setLhsAnim(0);
    manAnimationBlender.setRhsAnim(1);

    models[1]->setBones(&manAnimationBlender.bones());

    boneManager.setBindingPoint(0);
    boneManager.setShaderPrograms({colorShader, dirShadowShader, pointShadowShader});
    boneManager.setMaxModelsCount(2);
    boneManager.init();
    boneManager.getBlockBufferStorage().bind();
    boneManager.addModels({models[1], models[2]});
}

void ExampleChessContent::initLamps() {
    // configure LightingManager
    lightManager.setLightsLimit(dirLightsLimit, Light::Type::Dir);
    lightManager.setLightsLimit(pointLightsLimit, Light::Type::Point);
    lightManager.setLightsMapInitialSlot(6, Light::Type::Point);
    lightManager.setLightsMapInitialSlot(
            lightManager.getLightsMapInitialSlot(Light::Type::Point) + lightManager.getLightsLimit(Light::Type::Point),
            Light::Type::Dir);

    lightManager.setLightShader(colorShader);
    lightManager.setPointLightShadowShader(pointShadowShader);
    lightManager.setBindingPoint(1);
    lightManager.init();

    // create models
    ShapeCreator creator;
    creator.importFromFile(modelsPath "japanese_lamp/japanese_lamp.shape.json");

    auto lampShape = creator.get();

    lamps.resize(2); // TODO
    pointLamps.resize(1);
    dirLamps.resize(1);

    lamps[0] = PtrMaker::make();
    lamps[0]->setShape(lampShape);
    pointLamps[0].mptr = lamps[0];
    createPointLamp(pointLamps[0], {0.0f, 8.0f, 15.0f}, {1.0f, 1.0f, 1.0f}, 0);
    lamps[0]->translate();
    lamps[0]->transform();

    lamps[1] = PtrMaker::make();
    lamps[1]->setShape(lampShape);
    dirLamps[0].mptr = lamps[1];
    createDirLamp(dirLamps[0],
            {0.0f, 8.0f, -15.0f},
            {glm::radians(180.0f), glm::radians(30.0f), 0.0f},
            {253.0f / 255.0f, 184.0f / 255.0f, 19.0f / 255.0f}, 0);
    lamps[1]->translate();
    lamps[1]->transform();

    // write some lighting params
    lightManager.bindBuffer();
    lightManager.writePointLightsCount(pointLamps.size());
    lightManager.writeDirLightsCount(dirLamps.size());
    lightManager.writeShadowOpacity(shadowOpacity);
    lightManager.writeShadowDiskRadiusK(diskRadius_k);
    lightManager.writeShadowDiskRadiusMin(diskRadius_min);
    lightManager.unbindBuffer();
}

void ExampleChessContent::initShadowMaps() {
    colorShader->bind();

    lightManager.configureShadowMapping();

    for (usize i = 0; i < pointLamps.size(); i++)
        lightManager.pushShadowMap(pointLamps[i], i);

    for (usize i = 0; i < dirLamps.size(); i++)
        lightManager.pushShadowMap(dirLamps[i], i);

    colorShader->unbind();
}

void ExampleChessContent::initDOF() {
    dofCoCShader->bind();
    dofCoCShader->setFloat(COCShader::Vars::Cinematic::Aperture, dofAperture);
    dofCoCShader->setFloat(COCShader::Vars::Cinematic::ImageDistance, dofImageDistance);
    dofCoCShader->setFloat(COCShader::Vars::Cinematic::PlaneInFocus, -1.0f);
    dofCoCShader->unbind();
}

void ExampleChessContent::sendLampsData() {
    lightManager.bindBuffer();
    colorShader->setVec3(ColorShader::Vars::CameraPos, camera.getPos());

    for (size_t i = 0; i < pointLamps.size(); i++)
        lightManager.writePos(pointLamps[i], i);

    for (size_t i = 0; i < dirLamps.size(); i++)
        lightManager.writePos(dirLamps[i], i);

    lightManager.unbindBuffer();
}

glm::mat4 ExampleChessContent::getMVPMatrix(const glm::mat4 &modelMatrix) {
    return camera.getProjectionMatrix() * camera.getViewMatrix() * modelMatrix;
}

glm::mat4 ExampleChessContent::getMVMatrix(const glm::mat4 &modelMatrix) {
    return camera.getViewMatrix() * modelMatrix;
}

void ExampleChessContent::updateMatrices(const glm::mat4 &modelMatrix) {
    colorShader->setMat4(ColorShader::Vars::MVPMatrix, getMVPMatrix(modelMatrix));
    colorShader->setMat4(ColorShader::Vars::MVMatrix, getMVMatrix(modelMatrix));
    colorShader->setMat4(ColorShader::Vars::ModelMatrix, modelMatrix);
    colorShader->setMat4(ColorShader::Vars::ViewMatrix, camera.getViewMatrix());
}

void ExampleChessContent::drawModelDM(ModelPtr &model, ShaderProgramPtr &program, const glm::mat4 &mat) {
    auto &shape = model->getShape();
    shape->getInputLayout(0)->bind();

    boneManager.linkBuffer(model);

    program->setMat4(ShadowShader::Vars::TransformationMatrix, mat * model->transformation());

    for (auto &mesh : shape->getMeshes()) {
        Engine::drawElements(mesh.start, mesh.count);
    }
}

void ExampleChessContent::drawModel(ModelPtr &model) {
    auto &shape = model->getShape();
    shape->getInputLayout(1)->bind();

    boneManager.linkBuffer(model);

	updateMatrices(model->transformation());

    for (auto &mesh : shape->getMeshes()) {
        using namespace Module::Material::Vars;

        auto useNotNull = [](const Texture2DPtr &tex, const uint slot)
        {
            if (tex != nullptr) {
                tex->use(slot);
            } else {
                Engine::defaultTexture2D()->use(slot);
            }
        };

        const auto &material = mesh.material;
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

void ExampleChessContent::renderToDepthCubemap(uint index) {
    pointLamps[index].begin();
    pointLamps[index].updateMatrix();
    pointLamps[index].getShadowFramebuffer()->clearDepthBuffer();

    lightManager.pushShadowShaderPos(pointLamps[index]);
	lightManager.pushShadowShaderMatrices(pointLamps[index]);

	// drawing models
    for (auto &model : models)
        drawModelDM(model, pointShadowShader);

	// drawing lamps
	for (uint i = 0; i < pointLamps.size(); i++) {
		if (i == index)
		    continue;

        drawModelDM(pointLamps[i].mptr, pointShadowShader);
	}

	pointLamps[index].end();
}

void ExampleChessContent::renderToDepthMap(uint index) {
    dirLamps[index].begin();
    dirLamps[index].getShadowFramebuffer()->clearDepthBuffer();

	// drawing models
    for (auto &model : models)
        drawModelDM(model, dirShadowShader, dirLamps[index].getLightSpaceMatrix());

	// drawing lamps
	for (uint i = 0; i < dirLamps.size(); i++) {
		if (i == index)
		    continue;

        drawModelDM(dirLamps[i].mptr, dirShadowShader, dirLamps[index].getLightSpaceMatrix());
	}

	dirLamps[index].end();
}

void ExampleChessContent::renderScene() {
    displayFb->bind();
    displayFb->setActiveOutputList(0);
    displayFb->update();
    displayFb->clear(Framebuffer::ColorBuffer | Framebuffer::DepthBuffer);

    Engine::setViewport(width(), height());

    colorShader->bind();

    // sending lamps parameters to fragment shader
	sendLampsData();

    // drawing
    for (auto &model : models)
        drawModel(model);

	for (auto &lamp : lamps)
	    drawModel(lamp);

    // render skybox
    displayFb->setActiveOutputList(1);
    displayFb->update();

    Engine::setDepthTestMode(Engine::DepthTest::LessOrEqual);
    skyboxShader->bind();
    skyboxShader->setMat3(CubemapShader::Vars::ViewMatrix, glm::mat3(camera.getViewMatrix()));
    skyboxShader->setMat4(CubemapShader::Vars::TransformationMatrix, camera.getProjectionMatrix() * glm::mat4(glm::mat3(camera.getViewMatrix())));
    skybox->use(0);
    skyboxRenderer->getInputLayout()->bind();
    skyboxRenderer->draw();
    Engine::setDepthTestMode(Engine::DepthTest::Less);

    // postprocessing
    quadRenderer->getInputLayout()->bind();

    screenspaceFb->bind();
    ssrShader->bind();
    colorTex->use(0);
    normalTex->use(1);
    ssrValues->use(2);
    positionTex->use(3);
    quadRenderer->draw();

    Engine::setViewport(width() * bloomK, height() * bloomK);
    bloomSearchFb->bind();
    bloomSearchShader->bind();
    screenspaceTex->use(0);
    quadRenderer->draw();
    bloomBlur->makeBlur(bloomTex.get());

    Engine::setViewport(width() * dofK, height() * dofK);
    cocFb->bind();
    dofCoCShader->bind();
    positionTex->use(0);
    quadRenderer->draw();

    cocBlur->makeBlur(cocTex.get());
    dofBlur->makeBlur(screenspaceTex.get());
    Engine::setViewport(width(), height());

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
