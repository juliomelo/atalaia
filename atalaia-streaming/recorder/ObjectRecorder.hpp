#pragma once

#include "MotionRecorder.hpp"
#include "ObjectDetector.hpp"
#include <fstream>
#include "../notify/Notify.hpp"

class ObjectRecorder
{
    public:
        ObjectRecorder(Notifier *notifier) : notifier(notifier) {}
        virtual void process(string file);
        
    private:
        YoloV3ObjectDetector objectDetector;
        Notifier *notifier;
};

class ObjectRecordWriter
{
    public:
        ObjectRecordWriter(string file);
        virtual ~ObjectRecordWriter();

        void write(unsigned int frame, vector<DetectedObject> detectedObjects);

    private:
        ofstream fObjects;
};
