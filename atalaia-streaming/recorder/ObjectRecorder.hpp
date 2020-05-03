#pragma once

#include "MotionRecorder.hpp"
#include "../notify/Listener.hpp"
#include "ObjectDetector.hpp"

class ObjectRecorder : public Listener
{
    public:
        virtual void process(string file);
        
    private:
        YoloV3ObjectDetector objectDetector;
};

class FollowedObject : public DetectedObject
{
    
};
