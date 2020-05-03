#include "Notify.hpp"

using namespace std;

LocalNotifier::LocalNotifier(Listener *movements)
{
    this->movements = movements;
}

void LocalNotifier::notify(string filename, NotifyEvent event)
{
    switch (event)
    {
        case NotifyEvent::MOVEMENT:
            this->movements->enqueue(filename);
            break;
    }
}