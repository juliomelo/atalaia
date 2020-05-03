#include "Listener.hpp"

Listener::Listener() : queue(0, string(""))
{
    this->thread = new std::thread(this->threadProcess, this);
}

Listener::~Listener()
{
    delete this->thread;
    this->thread = NULL;
}

void Listener::waitShutdown(bool close)
{
    if (close)
        this->queue.close();

    this->queue.waitShutdown();
}

void Listener::threadProcess(Listener *listener)
{
    while (listener->thread) {
        string file = listener->queue.pop();

        if (file.length() == 0)
            break;

        listener->process(file);
    }

    listener->queue.close();
}