#include "MotionRecorder.hpp"
#include "MovementDetector.hpp"
#include <list>
#include <iostream>
#include <opencv2/highgui.hpp>

#define KEEP_PACKAGES_AFTER_KEYFRAME

using namespace std;

int Record::sequence = 0;

MotionRecorder::MotionRecorder(VideoStream *stream, Notifier *notifier, int maxSeconds)
{
    shutdown = false;
    this->stream = stream;
    this->notifier = notifier;
    this->maxSeconds = maxSeconds;
    thread = new std::thread(threadProcess, this);
}

MotionRecorder::~MotionRecorder()
{
    shutdown = true;
    this->thread->join();
    delete this->thread;
}

void MotionRecorder::threadProcess(MotionRecorder *recorder)
{
    MovementDetector movementDetector;
    std::list<AVPacket *> packetsFromKeyFrame;
    Record *record = NULL;
    int64_t dontStopUntil = 0;
    int64_t videoTimeThreshold = 0;
    int64_t lastKeyFrame = 0;
    std::list<AVPacket *> waitingMovement;

    do
    {
        FrameQueueItem *item = recorder->stream->getQueue()->pop();

        if (!item)
        {
           recorder->shutdown = true;
           break;
        }

        //imshow("thread", item->mat);
        //waitKey(25);

        DetectedMovements movements = item->mat.empty() ? DetectedMovements() : movementDetector.detectMovement(item->mat);

        if (item->packet->flags & AV_PKT_FLAG_KEY) // Keyframe
        {
            for (list<AVPacket *>::iterator it = packetsFromKeyFrame.begin(); it != packetsFromKeyFrame.end(); ++it)
                av_packet_unref(*it);

            packetsFromKeyFrame.clear();
            lastKeyFrame = item->packet->pts;

#ifndef KEEP_PACKAGES_AFTER_KEYFRAME
        packetsFromKeyFrame.push_back(av_packet_clone(item->packet));
#endif
        }

        if (!movements.empty())
        {
            for (int i = 0; i < movements.size(); i++)
            {
                 polylines(item->mat, movements[i].contour, true, Scalar(0, 0, 255));
            }

            // Mat show;
            // resize(item->mat, show, Size(640, 480));
            // imshow("movements", show);
            // waitKey(25);
            dontStopUntil = item->packet->pts + 3.0 / (item->time_base.num / (float)item->time_base.den);

            if (!record)
            {
                videoTimeThreshold = item->packet->pts + recorder->maxSeconds / (item->time_base.num / (float)item->time_base.den);
                record = new Record(recorder->stream->getAVStream());

                for (list<AVPacket *>::iterator it = packetsFromKeyFrame.begin(); it != packetsFromKeyFrame.end(); ++it)
                    record->writePacket(*it);
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
        }

#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
        packetsFromKeyFrame.push_back(av_packet_clone(item->packet));
#endif

        if (record && ((!item->mat.empty() && movements.empty() && item->packet->pts >= dontStopUntil) || (item->packet->flags & AV_PKT_FLAG_KEY && item->packet->pts >= videoTimeThreshold)))
        {
            string filename = record->getFileName();
            delete record;
            record = NULL;
            waitingMovement.clear();

            if (recorder->notifier)
                recorder->notifier->notify(filename, NotifyEvent::MOVEMENT);
        }
        else if (record && movements.empty())
            waitingMovement.push_back(av_packet_clone(item->packet));

        delete item;
    } while (!recorder->shutdown);

    if (record)
    {
        string filename = record->getFileName();
        delete record;
        if (recorder->notifier)
            recorder->notifier->notify(filename, NotifyEvent::MOVEMENT);
    }
}

Record::Record(AVStream *i_video_stream)
{
    this->i_video_stream = i_video_stream;
    std::ostringstream ss;

    ss << "data/local/" << sequence++ << ".atalaia";
    filename = ss.str();

    string video = filename + ".mp4";
    string data = filename + ".movements";

    this->data = fopen(data.c_str(), "w");

    avformat_alloc_output_context2(&this->o_fmt_ctx, NULL, NULL, video.c_str());

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

    avio_open(&o_fmt_ctx->pb, video.c_str(), AVIO_FLAG_WRITE);

    if (avformat_write_header(o_fmt_ctx, NULL) < 0)
    {
        cerr << "Can't write header.\n";
    }
}

Record::~Record()
{
    fclose(this->data);

    av_write_trailer(o_fmt_ctx);
    avcodec_close(o_fmt_ctx->streams[0]->codec);
    av_freep(&o_fmt_ctx->streams[0]->codec);
    av_freep(&o_fmt_ctx->streams[0]);
    avio_close(o_fmt_ctx->pb);
    av_free(o_fmt_ctx);
}

std::string Record::getFileName()
{
    return this->filename;
}

void Record::writePacket(AVPacket *packet, DetectedMovements *movements)
{
    AVPacket *outPacket = av_packet_clone(packet);
    outPacket->pts = av_rescale_q_rnd(outPacket->pts, this->i_video_stream->time_base, this->o_video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    outPacket->dts = av_rescale_q_rnd(outPacket->dts, this->i_video_stream->time_base, this->o_video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    outPacket->duration = av_rescale_q(outPacket->duration, this->i_video_stream->time_base, this->o_video_stream->time_base);
    outPacket->pos = -1;
    av_interleaved_write_frame(o_fmt_ctx, outPacket);

    unsigned long nMovements = movements ? movements->size() : 0;
    fwrite(&nMovements, sizeof(unsigned long), 1, this->data);

    for (int i = 0; i < nMovements; i++)
    {
        unsigned long nPoints = (*movements)[i].contour.size();

        fwrite(&nPoints, sizeof(unsigned long), 1, this->data);

        for (unsigned long j = 0; j < nPoints; j++)
            fwrite(&((*movements)[i].contour[j]), sizeof(Point), 1, this->data);
    }
}

MotionRecordReader::MotionRecordReader(string filename) : queue(1)
{
    string videoFilename = filename + ".mp4";
    this->video.start(videoFilename, &this->queue);

    string movementsFilename = filename + ".movements";
    this->fMovements = fopen(movementsFilename.c_str(), "r");
}

MotionRecordReader::~MotionRecordReader()
{
    fclose(this->fMovements);
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
            fread(&nMovements, sizeof(unsigned long), 1, this->fMovements);
            movementsDst = new DetectedMovements(nMovements);

            for (int i = 0; i < nMovements; i++)
            {
                unsigned long nPoints;
                fread(&nPoints, sizeof(unsigned long), 1, this->fMovements);
                vector<Point> contour(nPoints);

                for (unsigned long j = 0; j < nPoints; j++)
                {
                    Point point;
                    fread(&point, sizeof(Point), 1, this->fMovements);
                    contour.push_back(point);
                }

                movementsDst->push_back(DetectedMovement(contour));
            }
        }

        if (nMovements == 0)
            delete item;
    } while (nMovements == 0);

    return true;
}
