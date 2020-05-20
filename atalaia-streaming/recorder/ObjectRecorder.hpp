#pragma once

#include "MotionRecorder.hpp"
#include "../notify/Listener.hpp"
#include "ObjectDetector.hpp"
#include <fstream>

class ObjectRecorder : public Listener
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

        void write(unsigned int frame, DetectedObjects detectedObjects);

    private:
        ofstream fObjects;
};
