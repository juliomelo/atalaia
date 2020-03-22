#pragme once

#include <string>
#include "util/BlockingQueue.hpp"

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

class LocalNotifier : Notifier
{
    public:
        LocalNotifier();

    private:
        Listener movements;
        //Listener objects;
};
