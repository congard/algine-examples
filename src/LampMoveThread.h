#ifndef ALGINE_EXAMPLES_LAMPMOVETHREAD_H
#define ALGINE_EXAMPLES_LAMPMOVETHREAD_H

#include "LoopThread.h"

#include <glm/mat4x4.hpp>

class ExampleChessContent;

class LampMoveThread: public LoopThread {
public:
    LampMoveThread();

    void execute() override;

public:
    ExampleChessContent *content;

private:
    glm::mat3 rotate;
};


#endif //ALGINE_EXAMPLES_LAMPMOVETHREAD_H
