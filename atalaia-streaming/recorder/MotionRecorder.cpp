#include "MotionRecorder.hpp"
#include "MovementDetector.hpp"
#include <list>
#include <iostream>
#include <cstdio>
#include <opencv2/highgui.hpp>
#include "../main.hpp"

using namespace std;

int Record::sequence = 0;

MotionRecorder::MotionRecorder(VideoStream *stream, Notifier *notifier, int maxSeconds, bool notifyThis, int postStopSeconds)
{
    shutdown = false;
    this->stream = stream;
    this->notifier = notifier;
    this->maxSeconds = maxSeconds;
    this->notifyThis = notifyThis;
    this->postStopSeconds = postStopSeconds;
    thread = new std::thread(threadProcess, this);
}

MotionRecorder::~MotionRecorder()
{
    shutdown = true;
    this->thread->join();
    delete this->thread;
}

void MotionRecorder::setPassive(bool passive)
{
    std::lock_guard<std::mutex> lock(this->eventMutex);
    this->passive = passive;
}

void MotionRecorder::notify(TriggerKind kind, TriggerAction action)
{
    std::lock_guard<std::mutex> lock(this->eventMutex);

    bool *state = (kind == TriggerKind::MOVEMENT)
        ? &this->movementActive
        : &this->personActive;

    switch (action)
    {
        case TriggerAction::START:
            *state = true;
            this->stopRequested = false;
            this->notifyThis = true;
            break;

        case TriggerAction::STOP:
            *state = false;
            if (!this->movementActive && !this->personActive)
                this->stopRequested = true;
            break;

        case TriggerAction::PULSE:
        default:
            this->notifyThis = true;
            this->stopRequested = false;
            this->pulseRequested = true;
            break;
    }
}

bool MotionRecorder::hasActiveStartEvents()
{
    std::lock_guard<std::mutex> lock(this->eventMutex);
    return this->movementActive || this->personActive;
}

bool MotionRecorder::consumeStopRequested()
{
    std::lock_guard<std::mutex> lock(this->eventMutex);
    bool current = this->stopRequested;
    this->stopRequested = false;
    return current;
}

bool MotionRecorder::consumePulseRequested()
{
    std::lock_guard<std::mutex> lock(this->eventMutex);
    bool current = this->pulseRequested;
    this->pulseRequested = false;
    return current;
}

void MotionRecorder::clearStopRequested()
{
    std::lock_guard<std::mutex> lock(this->eventMutex);
    this->stopRequested = false;
}

void MotionRecorder::threadProcess(MotionRecorder *recorder)
{
    MovementDetector movementDetector;
#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
    std::list<AVPacket *> packetsFromKeyFrame;
#endif
    Record *record = NULL;
    int64_t dontStopUntil = 0;
    int64_t nextDetection = 0;
    int64_t videoTimeThreshold = 0;
    std::list<AVPacket *> waitingMovement;
    std::list<AVPacket *> postStopPackets;
    bool externalStartMode = false;
    bool waitingRestart = false;
    int64_t waitRestartUntil = 0;
#ifdef SHOW_MOVEMENT_DETECTION
    uint frames = 0;
#endif

    auto flushToRecord = [&record](std::list<AVPacket *> &packets) {
        for (list<AVPacket *>::iterator it = packets.begin(); it != packets.end(); it = packets.erase(it))
        {
            record->writePacket(*it);
            av_packet_unref(*it);
        }
    };

    auto releasePackets = [](std::list<AVPacket *> &packets) {
        for (list<AVPacket *>::iterator it = packets.begin(); it != packets.end(); it = packets.erase(it))
            av_packet_unref(*it);
    };

    auto finalizeRecord = [&](bool notify, bool resetNotifyThis) {
        if (!record)
            return;

        string filename = record->getFileName();
        int64_t duration = record->getDuration();
        bool keep = duration > 1;

        delete record;
        record = NULL;

        releasePackets(waitingMovement);
        releasePackets(postStopPackets);

        if (notify && recorder->notifier && keep)
        {
#ifdef VERBOSE
            cout << "Notifying movement " << filename << " - duration: " << duration << endl;
#endif
            recorder->notifier->notify(filename, NotifyEvent::MOVEMENT);
            movementDetector.reset();
        }
        else
        {
#ifdef VERBOSE
            cout << "Discarding " << filename << " - duration: " << duration << endl;
#endif

            string mp4 = filename + ".mp4";
            string movementsFilename = filename + ".movements";

#ifdef REMOVE_FILES
            remove(mp4.c_str());
            remove(movementsFilename.c_str());
#endif
        }

        if (resetNotifyThis)
            recorder->notifyThis = false;

        externalStartMode = false;
        waitingRestart = false;
        waitRestartUntil = 0;
    };

    do
    {
        FrameQueueItem *item = recorder->stream->getQueue()->pop();

        if (!item)
        {
           recorder->shutdown = true;
           break;
        }

        if (item->packet->flags & AV_PKT_FLAG_KEY) // Keyframe
        {
#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
            for (list<AVPacket *>::iterator it = packetsFromKeyFrame.begin(); it != packetsFromKeyFrame.end(); ++it)
                av_packet_unref(*it);

            packetsFromKeyFrame.clear();
#endif
        }

        bool hasActiveStart = recorder->hasActiveStartEvents();
        bool stopRequested = recorder->consumeStopRequested();
        bool pulseRequested = recorder->consumePulseRequested();

        if (!record && recorder->passive && !recorder->notifyThis && !hasActiveStart)
        {
            delete item;
            continue;
        }

        if (record && item->packet->pts >= videoTimeThreshold)
        {
            finalizeRecord(true, true);
        }

        if (record && externalStartMode)
        {
            if (pulseRequested)
            {
                waitingRestart = true;
                waitRestartUntil = item->packet->pts + recorder->postStopSeconds / (item->time_base.num / (float)item->time_base.den);
            }

            if (stopRequested && !hasActiveStart)
            {
                waitingRestart = true;
                waitRestartUntil = item->packet->pts + recorder->postStopSeconds / (item->time_base.num / (float)item->time_base.den);
            }

            if (waitingRestart)
            {
                if (hasActiveStart || recorder->notifyThis)
                {
                    flushToRecord(postStopPackets);
                    waitingRestart = false;
                    recorder->clearStopRequested();
                }
                else if (item->packet->pts < waitRestartUntil)
                {
                    postStopPackets.push_back(av_packet_clone(item->packet));
                    delete item;
                    continue;
                }
                else
                {
                    finalizeRecord(true, true);
                    delete item;
                    continue;
                }
            }

            if (record)
            {
                record->writePacket(item->packet);
                delete item;
                continue;
            }
        }

        if (!record && hasActiveStart)
        {
            videoTimeThreshold = item->packet->pts + recorder->maxSeconds / (item->time_base.num / (float)item->time_base.den);
            record = new Record(recorder->stream->getAVStream());
            externalStartMode = true;
            waitingRestart = false;

#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
            for (list<AVPacket *>::iterator it = packetsFromKeyFrame.begin(); it != packetsFromKeyFrame.end(); ++it)
                record->writePacket(*it);
#endif

            if (!postStopPackets.empty())
                flushToRecord(postStopPackets);

            record->writePacket(item->packet);
            delete item;
            continue;
        }

        if (!record && pulseRequested)
        {
            videoTimeThreshold = item->packet->pts + recorder->maxSeconds / (item->time_base.num / (float)item->time_base.den);
            record = new Record(recorder->stream->getAVStream());
            externalStartMode = true;
            waitingRestart = true;
            waitRestartUntil = item->packet->pts + recorder->postStopSeconds / (item->time_base.num / (float)item->time_base.den);

#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
            for (list<AVPacket *>::iterator it = packetsFromKeyFrame.begin(); it != packetsFromKeyFrame.end(); ++it)
                record->writePacket(*it);
#endif

            if (!postStopPackets.empty())
                flushToRecord(postStopPackets);

            record->writePacket(item->packet);
            delete item;
            continue;
        }

        DetectedMovements movements = (item->mat.empty() || (!record && nextDetection && item->packet->pts < nextDetection))
            ? DetectedMovements()
            : movementDetector.detectMovement(item->mat);

#ifdef SHOW_MOVEMENT_DETECTION
        if (frames++ % 15 == 0 || !movements.empty()) {
            for (size_t i = 0; i < movements.size(); i++)
            {
                    polylines(item->mat, movements[i].contour, true, Scalar(0, 0, 255));
                    rectangle(item->mat, movements[i], Scalar(255, 0, 0));
            }

            Mat show;
            resize(item->mat, show, Size(480, 320));
            imshow("movement", show);
            waitKey(25);
        }
#endif

        if (!movements.empty() || (recorder->notifyThis && (!record || record->getDuration() < 1)))
        {
#ifdef VERBOSE
            if (recorder->notifyThis && !record)
                cout << "[*] Forcing movement record notification" << endl;
#endif

            dontStopUntil = item->packet->pts + 3.0 / (item->time_base.num / (float)item->time_base.den);

            if (!record)
            {
                videoTimeThreshold = item->packet->pts + recorder->maxSeconds / (item->time_base.num / (float)item->time_base.den);
                record = new Record(recorder->stream->getAVStream());

#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
                for (list<AVPacket *>::iterator it = packetsFromKeyFrame.begin(); it != packetsFromKeyFrame.end(); ++it)
                    record->writePacket(*it);
#endif
            }
            else
            {
                for (list<AVPacket *>::iterator it = waitingMovement.begin(); it != waitingMovement.end(); it = waitingMovement.erase(it))
                {
                    record->writePacket(*it);
                    av_packet_unref(*it);
                }
            }

            record->writePacket(item->packet, &movements);
        } else if (!record && !item->mat.empty() && item->packet->pts > nextDetection) {
            nextDetection = item->packet->pts + MOVEMENT_PERIOD_WATCH / (item->time_base.num / (float)item->time_base.den);
        }

#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
        packetsFromKeyFrame.push_back(av_packet_clone(item->packet));
#endif

        if (record &&
            (
                (!item->mat.empty() && movements.empty() && item->packet->pts >= dontStopUntil)
                || (item->packet->flags & AV_PKT_FLAG_KEY && item->packet->pts >= videoTimeThreshold)
            )
            && (recorder->notifyThis == false || record->getDuration() >= 1)
        )
        {
            finalizeRecord(true, true);
        }
        else if (record && recorder->notifyThis && record->getDuration() < 1)
            record->writePacket(item->packet);
        else if (record && movements.empty())
        {
            waitingMovement.push_back(item->packet);
            item->packet = NULL;
        }

        delete item;
    } while (!recorder->shutdown);

    if (record)
    {
        string filename = record->getFileName();
        int64_t duration = record->getDuration();
        bool keep = duration > 1;

        delete record;

        releasePackets(waitingMovement);
        releasePackets(postStopPackets);

        if (recorder->notifier && keep)
            recorder->notifier->notify(filename, NotifyEvent::MOVEMENT);
    }
}

Record::Record(AVStream *i_video_stream)
{
    time_t now;
    this->frames = 0;
    this->i_video_stream = i_video_stream;
    this->duration = 0;
    std::ostringstream ss;

    time(&now);

    ss << "data/local/" << sequence++ << "-" << now << ".atalaia";
    filename = ss.str();

    string video = filename + ".mp4";
    string data = filename + ".movements";

    this->data.open(data);

    if (avformat_alloc_output_context2(&this->o_fmt_ctx, NULL, NULL, video.c_str()) < 0)
    {
        cerr << "Can't create output context." << endl;
    }

#ifdef VERBOSE
    cout << "Creating " << video << endl;
#endif

    /*
    * since all input files are supposed to be identical (framerate, dimension, color format, ...)
    * we can safely set output codec values from first input file
    */
    this->o_video_stream = avformat_new_stream(o_fmt_ctx, 0);
    {
        AVCodecParameters *c;
        c = o_video_stream->codecpar;
        avcodec_parameters_copy(c, i_video_stream->codecpar);
    }

    if (avio_open(&o_fmt_ctx->pb, video.c_str(), AVIO_FLAG_WRITE) < 0)
    {
        cerr << "Can't open output file." << endl;
    }

    if (avformat_write_header(o_fmt_ctx, NULL) < 0)
    {
        cerr << "Can't write header." << endl;
    }
}

Record::~Record()
{
    this->data.close();

    av_write_trailer(o_fmt_ctx);
    avcodec_close(o_fmt_ctx->streams[0]->codec);
    av_freep(&o_fmt_ctx->streams[0]->codec);
    av_freep(&o_fmt_ctx->streams[0]);
    avio_close(o_fmt_ctx->pb);
    avformat_free_context(this->o_fmt_ctx);
//    av_free(o_fmt_ctx);
}

std::string Record::getFileName()
{
    return this->filename;
}

void Record::writePacket(AVPacket *packet, DetectedMovements *movements)
{
    // If first packet is keyframe, then use its PTS/DTS as base, but, if not, then assume 0 and calculate based on fps
    if (this->frames == 0 && packet->dts < 0)
    {
        this->first_pts = 0;
        this->first_dts = 0;
    }
    else
#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
    if (this->frames == 0)  // Keyframe
    {
        this->first_pts = packet->pts;
        this->first_dts = packet->dts;
    }
#endif

    this->frames++;

    AVPacket *outPacket = av_packet_clone(packet);

    if (this->first_pts == 0)
    {
        AVRational fps = this->i_video_stream->r_frame_rate;
        int64_t frame_duration = av_rescale_q(1, av_inv_q(fps), this->i_video_stream->time_base);

        outPacket->pts = frame_duration * (this->frames - 1);
        outPacket->dts = frame_duration * (this->frames - 1);
    }
    else
    {
        outPacket->pts = av_rescale_q_rnd(packet->pts - first_pts, this->i_video_stream->time_base, this->o_video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        outPacket->dts = av_rescale_q_rnd(packet->dts - first_dts, this->i_video_stream->time_base, this->o_video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    }
    
    outPacket->duration = av_rescale_q(packet->duration, this->i_video_stream->time_base, this->o_video_stream->time_base);
    outPacket->pos = -1;
    //av_interleaved_write_frame(o_fmt_ctx, outPacket);
    av_write_frame(o_fmt_ctx, outPacket);

    this->duration = av_rescale_q(outPacket->pts, this->o_video_stream->time_base, {1, 1000}) / 1000.;

    unsigned long nMovements = movements ? movements->size() : 0;
    this->data << nMovements << endl;

    for (unsigned long i = 0; i < nMovements; i++)
    {
        unsigned long nPoints = (*movements)[i].contour.size();

        this->data << nPoints;

        for (unsigned long j = 0; j < nPoints; j++)
        {
            Point p = (*movements)[i].contour[j];
            this->data << " " << p.x << " " << p.y;
        }

        this->data << endl;
    }

    av_packet_unref(outPacket);
}

MotionRecordReader::MotionRecordReader(string filename) : queue(1)
{
    string videoFilename = filename + ".mp4";
    this->video.start(videoFilename, &this->queue);

    string movementsFilename = filename + ".movements";
    this->fMovements.open(movementsFilename);
}

MotionRecordReader::~MotionRecordReader()
{
    this->fMovements.close();
    this->queue.close();
}

bool MotionRecordReader::readNext(FrameQueueItem *&item, DetectedMovements *&movementsDst)
{
    unsigned long nMovements = 0;

    do
    {
        /* TO-DO: We don't need to decode every frame, but we could skip
         * all frames before keyframe, if there is such one new before
         * movement start.
         */
        item = this->queue.pop();

        if (!item)
            return false;

        if (!item->mat.empty())
        {
            try
            {
                this->fMovements >> nMovements;

                movementsDst = new DetectedMovements(nMovements);

                for (unsigned long i = 0; i < nMovements; i++)
                {
                    unsigned long nPoints;
                    this->fMovements >> nPoints;

                    vector<Point> contour(nPoints);

                    for (unsigned long j = 0; j < nPoints; j++)
                    {
                        Point point;

                        this->fMovements >> point.x >> point.y;

                        contour[j] = point;
                    }

                    (*movementsDst)[i] = DetectedMovement(contour);
                }
            }
            catch (std::exception &e)
            {
                std::cout << e.what() << std::endl;
                return false;
            }
        }

        if (nMovements == 0)
            delete item;
    } while (nMovements == 0);

    return true;
}
