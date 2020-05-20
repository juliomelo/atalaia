#pragma once

#include <vector>
#include <thread>
#include "../util/BlockingQueue.hpp"
#include <opencv2/imgcodecs.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
}

#define BUFFER_SECONDS 15

using namespace std;

class FrameQueueItem
{
    public:
        ~FrameQueueItem();
        AVPacket *packet;
        cv::Mat mat;
        AVRational time_base;
        unsigned int frameCount;
};

enum VideoState
{
    WAITING,
    PROCESSING,
    SHUTDOWN,
    FAILURE
};

typedef BlockingQueue<FrameQueueItem *> VideoStreamQueue;

class VideoStream
{
    public:
        VideoStream();
        virtual ~VideoStream();
        int start(std::string url, VideoStreamQueue *queue);
        vector<AVPacket> getPacketsSince(int64_t pts);
        inline VideoStreamQueue *getQueue() { return this->queue; }
        inline AVStream *getAVStream() { return this->vstrm; }
        inline std::string getUrl() { return this->url; }
        inline VideoState getState() { return this->threadState; }

    private:
        std::thread *thread;
        std::string url;
        VideoState threadState;
        VideoStreamQueue *queue;
        AVFormatContext *format_ctx;
        AVStream *vstrm;
        std::vector<uint8_t> framebuf;

        static void threadProcess(VideoStream *url);
};
