#pragma once

#include "MotionRecorder.hpp"
#include "../notify/Listener.hpp"
#include "ObjectDetector.hpp"

class ObjectRecorder : public Listener
{
    public:
        ObjectRecorder(Notifier *notifier) : notifier(notifier) {}
        virtual void process(string file);
        
    private:
        YoloV3ObjectDetector objectDetector;
        Notifier *notifier;
};

class FollowedObject : public DetectedObject
{
    
};
