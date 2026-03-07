#pragma once

#include "VideoStream.hpp"
#include <thread>
#include <list>
#include "MovementDetector.hpp"
#include "../notify/Notify.hpp"
#include <fstream>
#include <mutex>

using namespace std;

enum class TriggerKind
{
    MOVEMENT,
    PERSON
};

enum class TriggerAction
{
    PULSE,
    START,
    STOP
};

class MotionRecorder
{
    public:
        MotionRecorder(VideoStream *stream, Notifier *notifier, int maxSeconds = 10, bool notifyThis = false, int postStopSeconds = 5);
        ~MotionRecorder();
        void notify(TriggerKind kind, TriggerAction action);
        void setPassive(bool passive);

        /**
         * Force notify this record.
         */
        bool notifyThis = false;
        bool passive = false;

    private:
        VideoStream *stream;
        std::thread *thread;
        bool shutdown;
        int maxSeconds;
        int postStopSeconds;
        bool movementActive = false;
        bool personActive = false;
        bool stopRequested = false;
        bool pulseRequested = false;
        std::mutex eventMutex;

        bool hasActiveStartEvents();
        bool consumeStopRequested();
        bool consumePulseRequested();
        void clearStopRequested();

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
        inline int64_t getDuration() { return duration; }

    private:
        static int sequence;
        AVFormatContext *o_fmt_ctx;
        AVStream *i_video_stream;
        AVStream *o_video_stream;
        std::ofstream data;
        std::string filename;
        int64_t frames;
        int64_t first_pts;
        int64_t first_dts;
        int64_t duration;
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
        std::ifstream fMovements;
};
