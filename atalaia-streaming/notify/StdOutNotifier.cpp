#include "StdOutNotifier.hpp"
#include <iostream>

using namespace std;

StdOutNotifier::StdOutNotifier(VideoStream *stream, bool single)
{
    this->stream = stream;
    this->single = single;
}

void StdOutNotifier::notify(string filename, NotifyEvent event, string arg)
{
    string ev;
    switch (event)
    {
        case NotifyEvent::MOVEMENT:
            ev = "movement";
            break;
        case NotifyEvent::OBJECT:
            ev = "object";
            break;
        default:
            ev = "unknown";
            break;
    }

    if (arg.empty())
    {
        cout << ev << "\t" << filename << endl;
    }
    else
    {
        cout << ev << "\t" << filename << "\t" << arg << endl;
    }

    if (this->single)
    {
        this->done = true;
        this->stream->getQueue()->close();
    }
}