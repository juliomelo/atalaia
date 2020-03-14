#pragma once

#include "VideoStream.hpp"
#include <thread>
#include <list>
#include "MovementDetector.hpp"

using namespace std;

class MotionRecorder
{
    public:
        MotionRecorder(VideoStream *stream);
        ~MotionRecorder();

    private:
        VideoStream *stream;
        std::thread *thread;
        bool shutdown;

        static void
        threadProcess(MotionRecorder *recorder);
        std::string newFileName();
};

struct frame_record
{
    int64_t pts;            // Presentation timestamp in AVStream->time_base units; the time at which the decompressed packet will be presented to the user.
    int     size;
    uint8_t data[];
};

struct point
{
    int x, y;
};

struct movement_record
{
    uint8_t frameSize;
    int64_t pts;
    unsigned long totalPoints;
    point   points[];
};

class Record
{
    public:
        Record(AVStream *i_video_stream);
        ~Record();
        std::string getFileName();
        void writePacket(AVPacket *packet, DetectedMovements *movements = NULL);

    private:
        static int sequence;
        AVFormatContext *o_fmt_ctx;
        AVStream *i_video_stream;
        AVStream *o_video_stream;
        FILE *data;
        std::string filename;
};
