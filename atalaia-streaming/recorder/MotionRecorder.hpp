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

class MotionRecordReader
{
    public:
        MotionRecordReader(std::string filename);
        bool readNext(FrameQueueItem *&item, DetectedMovements &movementsDst);

    private:
        VideoStream video;
        VideoStreamQueue queue;
        FILE *fMovements;
};
