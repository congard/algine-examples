#include "LoopThread.h"

LoopThread::LoopThread()
    : m_isLoopRunning(true) {}

void LoopThread::startLoop() {
    m_thread = std::thread([&]() {
        while (isLoopRunning()) {
            execute();
        }
    });
}

void LoopThread::stopLoop() {
    m_isLoopRunning = false;

    m_thread.join();
}

const std::atomic_bool& LoopThread::isLoopRunning() const {
    return m_isLoopRunning;
}
