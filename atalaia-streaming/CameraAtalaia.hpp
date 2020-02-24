#pragma once

#include "util/BlockingQueue.hpp"
#include "VideoStream.hpp"
#include "ObjectDetector.hpp"
#include <set>
#include <thread>

class CameraAtalaia
{
    public:
        CameraAtalaia(std::string);
        ~CameraAtalaia();
        void record(AVPacket *packet);

    private:
        bool running;
        BlockingQueue<FrameQueueItem *> queue;
        VideoStream stream;
        std::thread *thread;

        static void threadProcess(CameraAtalaia *camera);
        static std::set<string> objectOfInterest;
};

enum CameraState
{
    NORMAL,
    DETECTED_OBJECT_OF_INTEREST
};
