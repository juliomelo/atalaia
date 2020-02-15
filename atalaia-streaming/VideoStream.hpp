#pragma once

#include <thread>
#include "util/BlockingQueue.hpp"
#include <opencv2/imgcodecs.hpp>
#include "MovementDetector.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
}

#define BUFFER_SECONDS 15

class FrameQueueItem
{
    public:
        ~FrameQueueItem();
        AVPacket *packet;
        cv::Mat mat;
        DetectedMovements movements;
};

class VideoStream
{
    public:
        VideoStream(BlockingQueue<FrameQueueItem *> *queue);
        ~VideoStream();
        int start(char *url);

    private:
        std::thread *thread;
        char *url;
        int threadState;
        BlockingQueue<FrameQueueItem *> *queue;

        static void threadProcess(VideoStream *url);
};
