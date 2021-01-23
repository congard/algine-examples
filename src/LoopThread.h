#ifndef ALGINE_EXAMPLES_LOOPTHREAD_H
#define ALGINE_EXAMPLES_LOOPTHREAD_H

#include <thread>
#include <atomic>

class LoopThread {
public:
    LoopThread();

    virtual void execute() = 0;

    void startLoop();
    void stopLoop();

    const std::atomic_bool& isLoopRunning() const;

private:
    std::thread m_thread;
    std::atomic_bool m_isLoopRunning;
};

#endif //ALGINE_EXAMPLES_LOOPTHREAD_H
