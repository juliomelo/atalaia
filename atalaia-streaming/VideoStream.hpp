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

class CircularPacketArray
{
    public:
        CircularPacketArray(AVRational frame_rate);
        CircularPacketArray(int buffer_size);
        ~CircularPacketArray();
        void next();
        AVPacket *current();
        vector<AVPacket> getPacketsSince(int64_t pts);
        int size();
        bool full;

    private:
        int buffer_size;
        AVPacket *packets;
        int currentIndex;
};

class FrameQueueItem
{
    public:
        ~FrameQueueItem();
        AVPacket *packet;
        cv::Mat mat;
        DetectedMovements movements;
        AVRational time_base;
};

enum VideoState
{
    WAITING,
    PROCESSING,
    RECORDING,
    SHUTDOWN,
    FAILURE
};

class VideoStream
{
    public:
        VideoStream(BlockingQueue<FrameQueueItem *> *queue);
        ~VideoStream();
        int start(std::string url);
        vector<AVPacket> getPacketsSince(int64_t pts);

    private:
        std::thread *thread;
        std::string url;
        VideoState threadState;
        BlockingQueue<FrameQueueItem *> *queue;
        CircularPacketArray *packets;

        static void threadProcess(VideoStream *url);
};
