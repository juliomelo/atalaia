#pragma once

#include <string>

using namespace std;

enum NotifyEvent
{
    MOVEMENT,
    OBJECT
};

class Notifier
{
    public:
        virtual void notify(string filename, NotifyEvent event, string arg = "") = 0;
};
