#pragma once

#include "Notify.hpp"
#include "../recorder/VideoStream.hpp"

class StdOutNotifier : public Notifier
{
    public:
        StdOutNotifier(VideoStream *stream, bool single = false);
        virtual void notify(string filename, NotifyEvent event, string arg = "");
        bool done = 0;

    private:
        VideoStream *stream;
        bool single;
};
