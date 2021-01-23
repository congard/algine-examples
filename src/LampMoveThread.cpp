#define GLM_FORCE_CTOR_INIT

#include "LampMoveThread.h"

#include "ExampleChessContent.h"

#include <algine/std/model/Model.h>

#include <glm/gtc/matrix_transform.hpp>

#include <thread>

using namespace std;

LampMoveThread::LampMoveThread()
    : content(nullptr)
{
    rotate = glm::mat3(glm::rotate(glm::mat4(), glm::radians(0.01f), glm::vec3(0, 1, 0)));
}

void LampMoveThread::execute() {
    this_thread::sleep_for(chrono::milliseconds(1));

    content->pointLamps[0].setPos(content->pointLamps[0].m_pos * rotate);
    content->pointLamps[0].translate();
    content->pointLamps[0].mptr->transform();
}
