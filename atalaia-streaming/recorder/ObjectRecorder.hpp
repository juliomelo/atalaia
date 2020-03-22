#pragma once

#include "MotionRecorder.hpp"
#include "../notify/Listener.hpp"
#include "ObjectDetector.hpp"

class ObjectRecorder : public Listener
{
    private:
        YoloV3ObjectDetector objectDetector;
        void process(string file);
};

class FollowedObject : public DetectedObject
{
    
};
