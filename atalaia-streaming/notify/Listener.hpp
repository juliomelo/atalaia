#pragma once

#include <string>
#include "../util/BlockingQueue.hpp"
#include <thread>

using namespace std;

class Listener
{
    public:
        Listener();
        virtual ~Listener();
        inline bool enqueue(string file) { return this->queue.try_push(file); }

    protected:
        virtual void process(string file) = 0;

    private:
        std::thread *thread;
        BlockingQueue<string> queue;

        static void threadProcess(Listener *listener);
};
