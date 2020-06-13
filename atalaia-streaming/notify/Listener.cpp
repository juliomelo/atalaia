#include "Listener.hpp"
#include "../main.hpp"

Listener::Listener() : queue(0, string(""))
{
    this->thread = new std::thread(this->threadProcess, this);
}

Listener::~Listener()
{
    this->queue.close();
    this->queue.waitShutdown();
    this->thread = NULL;
    delete this->thread;
}

void Listener::waitShutdown(bool close)
{
    if (close)
        this->queue.close();

    this->queue.waitShutdown();
    this->thread->join();
}

void Listener::threadProcess(Listener *listener)
{
    while (!terminating && listener->thread) {
        string file = listener->queue.pop();

        if (file.length() == 0)
            break;

        listener->process(file);
    }

    listener->queue.close();
}