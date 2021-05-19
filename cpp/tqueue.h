#ifndef TQUEUE_H
#define TQUEUE_H

#include <thread>
#include <mutex>
#include <condition_variable>

template<typename T>
class tqueue
{
public:
    tqueue(int bufSize = 0x100) {
        buf = new T[bufSize];
        this->bufSize = bufSize;
        headIdx = tailIdx = 0;
    }
    ~tqueue() {
        delete buf;
    }
    void push(T item) {
        mutex.lock();

        std::unique_lock<std::mutex> lock(mutex);
        while (isFull()) condPop.wait(lock);

        buf[tailIdx] = item;
        tailIdx = (tailIdx + 1) % bufSize;

        mutex.unlock();
        condPush.notify_all();
    }
    T pop() {
        mutex.lock();

        std::unique_lock<std::mutex> lock(mutex);
        while (isEmpty()) condPush.wait(lock);

        T item = buf[headIdx];
        headIdx = (headIdx + 1) % bufSize;

        mutex.unlock();
        condPop.notify_all();

        return item;
    }
private:
    T *buf;
    int bufSize, headIdx, tailIdx;
    std::mutex mutex;
    std::condition_variable condPush, condPop;

    bool isFull() {
        return (tailIdx + 1) % bufSize == headIdx;
    }
    bool isEmpty() {
        return headIdx == tailIdx;
    }
};

#endif

