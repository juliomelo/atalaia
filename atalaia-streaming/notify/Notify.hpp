#pragma once

#include <string>
#include "../util/BlockingQueue.hpp"
#include "Listener.hpp"

using namespace std;

enum NotifyEvent
{
    MOVEMENT,
    OBJECT
};

class Notifier
{
    public:
        virtual void notify(string filename, NotifyEvent event) = 0;
};

class LocalNotifier : public Notifier
{
    public:
        LocalNotifier(Listener *movements);

    private:
        Listener *movements;
        //Listener objects;
        virtual void notify(string filename, NotifyEvent event);
};
