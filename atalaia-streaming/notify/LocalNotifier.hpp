#pragma once

#include "Listener.hpp"
#include "Notify.hpp"
#include "../recorder/ObjectRecorder.hpp"

class MovementListener : public Listener
{
    public:
        MovementListener() : objectRecorder(NULL) {}

    private:
        ObjectRecorder objectRecorder;

    public:
        virtual void process(string file)
        {
            objectRecorder.process(file);
        }
};

class LocalNotifier : public Notifier
{
    public:
        LocalNotifier() {}
        void waitShutdown();

    private:
        MovementListener movementListener;
        virtual void notify(string filename, NotifyEvent event, string arg = "");
};

