#ifndef ALGINE_EXAMPLES_EXAMPLECHESSCONTENT_H
#define ALGINE_EXAMPLES_EXAMPLECHESSCONTENT_H

#include <algine/core/window/WindowEventHandler.h>

#include <algine/core/Content.h>

#include <algine/std/model/ShapePtr.h>
#include <algine/std/model/ModelPtr.h>
#include <algine/std/animation/AnimationBlender.h>
#include <algine/std/animation/BoneSystemManager.h>
#include <algine/std/CubeRendererPtr.h>
#include <algine/std/QuadRendererPtr.h>
#include <algine/std/Camera.h>
#include <algine/std/rotator/EulerRotator.h>

#include <algine/ext/lighting/PointLamp.h>
#include <algine/ext/lighting/DirLamp.h>
#include <algine/ext/lighting/LightingManager.h>
#include <algine/ext/Blur.h>

#include <vector>

#include "LampMoveThread.h"

using namespace algine;

class ExampleChessContent: public Content, public WindowEventHandler {
    friend class LampMoveThread;

public:
    ~ExampleChessContent() override;

    void init() override;

    void render() override;

    void mouseMove(double x, double y) override;
    void mouseClick(MouseKey key) override;

    void keyboardKeyPress(KeyboardKey key) override;

    void windowSizeChange(int width, int height) override;

private:
    void resize();
    void pollKeys();

    void createPointLamp(PointLamp &result, const glm::vec3 &pos, const glm::vec3 &color, usize id);
    void createDirLamp(DirLamp &result, const glm::vec3 &pos, const glm::vec3 &rotate, const glm::vec3 &color, usize id);

    void initShaders();
    void initCamera();
    void createModels();
    void initLamps();
    void initShadowMaps();
    void initDOF();

    void sendLampsData();

    glm::mat4 getMVPMatrix(const glm::mat4 &modelMatrix);
    glm::mat4 getMVMatrix(const glm::mat4 &modelMatrix);
    void updateMatrices(const glm::mat4 &modelMatrix);

    void drawModelDM(ModelPtr &model, ShaderProgramPtr &program, const glm::mat4 &mat = glm::mat4(1.0f));
    void drawModel(ModelPtr &model);
    void renderToDepthCubemap(uint index);
    void renderToDepthMap(uint index);

    void renderScene();

private:
    std::vector<ShapePtr> shapes;
    std::vector<ModelPtr> models, lamps;
    AnimationBlender manAnimationBlender;
    BoneSystemManager boneManager;

private:
    std::vector<PointLamp> pointLamps;
    std::vector<DirLamp> dirLamps;
    LampMoveThread lampThread;
    LightingManager lightManager;

private:
    CubeRendererPtr skyboxRenderer;
    QuadRendererPtr quadRenderer;

private:
    Ptr<Blur> bloomBlur;
    Ptr<Blur> dofBlur;
    Ptr<Blur> cocBlur;

private:
    FramebufferPtr displayFb;
    FramebufferPtr screenspaceFb;
    FramebufferPtr bloomSearchFb;
    FramebufferPtr cocFb;

private:
    Texture2DPtr colorTex;
    Texture2DPtr normalTex;
    Texture2DPtr ssrValues;
    Texture2DPtr positionTex;
    Texture2DPtr screenspaceTex;
    Texture2DPtr bloomTex;
    Texture2DPtr cocTex;
    TextureCubePtr skybox;

private:
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

private:
    Camera camera;
    glm::vec2 lastMousePos {0, 0};

private:
    EulerRotator manHeadRotator;
};

#endif //ALGINE_EXAMPLES_EXAMPLECHESSCONTENT_H
