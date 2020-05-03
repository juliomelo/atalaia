#pragma once

#include "VideoStream.hpp"
#include <thread>
#include <list>
#include "MovementDetector.hpp"
#include "../notify/Notify.hpp"

using namespace std;

class MotionRecorder
{
    public:
        MotionRecorder(VideoStream *stream, Notifier *notifier, int maxSeconds = 5);
        ~MotionRecorder();

    private:
        VideoStream *stream;
        std::thread *thread;
        bool shutdown;
        int maxSeconds;

        static void threadProcess(MotionRecorder *recorder);
        std::string newFileName();
        Notifier *notifier;

};

class Record
{
    public:
        Record(AVStream *i_video_stream);
        virtual ~Record();
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
        virtual ~MotionRecordReader();
        bool readNext(FrameQueueItem *&item, DetectedMovements *&movementsDst);

    private:
        VideoStream video;
        VideoStreamQueue queue;
        FILE *fMovements;
};
