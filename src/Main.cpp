#include <algine/core/window/Window.h>

#include <algine/core/Engine.h>

#include "ExampleChessContent.h"

int main() {
    Engine::init();

    Window window("Algine", 1366, 768);
    window.create();
    window.setFullscreenDimensions(1366, 768);
    window.setIcon(Icon("src/resources/icon64x64.png"));
    window.setMouseTracking(true);
    window.setKeyboardTracking(true);
    window.setWindowStateTracking(true);
    window.setCursorMode(Window::CursorMode::Disabled);
    window.setContent(new ExampleChessContent);
    window.renderLoop();

    Engine::destroy();

    return 0;
}
