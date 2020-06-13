#include "LocalNotifier.hpp"

using namespace std;

void LocalNotifier::notify(string filename, NotifyEvent event, string arg)
{
    switch (event)
    {
        case NotifyEvent::MOVEMENT:
            this->movementListener.enqueue(filename);
            break;
    }
}

void LocalNotifier::waitShutdown()
{
    this->movementListener.waitShutdown(true);
}
