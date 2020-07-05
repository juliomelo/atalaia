#include "MotionRecorder.hpp"
#include "MovementDetector.hpp"
#include <list>
#include <iostream>
#include <opencv2/highgui.hpp>
#include "../main.hpp"

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
#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
    std::list<AVPacket *> packetsFromKeyFrame;
#endif
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

        DetectedMovements movements = item->mat.empty() ? DetectedMovements() : movementDetector.detectMovement(item->mat);

        if (item->packet->flags & AV_PKT_FLAG_KEY) // Keyframe
        {
#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
            for (list<AVPacket *>::iterator it = packetsFromKeyFrame.begin(); it != packetsFromKeyFrame.end(); ++it)
                av_packet_unref(*it);

            packetsFromKeyFrame.clear();
#endif

            lastKeyFrame = item->packet->pts;
        }

#ifdef SHOW_MOVEMENT_DETECTION
        for (int i = 0; i < movements.size(); i++)
        {
                polylines(item->mat, movements[i].contour, true, Scalar(0, 0, 255));
                rectangle(item->mat, movements[i], Scalar(255, 0, 0));
        }

        Mat show;
        resize(item->mat, show, Size(480, 320));
        imshow("movement", show);
        waitKey(25);
#endif

        if (!movements.empty())
        {
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
        }

#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
        packetsFromKeyFrame.push_back(av_packet_clone(item->packet));
#endif

        if (record && ((!item->mat.empty() && movements.empty() && item->packet->pts >= dontStopUntil) || (item->packet->flags & AV_PKT_FLAG_KEY && item->packet->pts >= videoTimeThreshold)))
        {
            string filename = record->getFileName();
            int64_t duration = record->getDuration();
            bool keep = duration > 1;

            delete record;
            record = NULL;

            for (list<AVPacket *>::iterator it = waitingMovement.begin(); it != waitingMovement.end(); it = waitingMovement.erase(it))
                av_packet_unref(*it);

            if (recorder->notifier && keep)
                recorder->notifier->notify(filename, NotifyEvent::MOVEMENT);
            else
            {
                cout << "Discarding " << filename << " - duration: " << duration << endl;

                string mp4 = filename + ".mp4";
                string movementsFilename = filename + ".movements";

#ifdef REMOVE_FILES
                remove(mp4.c_str());
                remove(movementsFilename.c_str());
#endif
            }
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
#ifdef KEEP_PACKAGES_AFTER_KEYFRAME
    if (this->frames++ == 0)  // Keyframe
    {
        this->first_pts = packet->pts;
        this->first_dts = packet->dts;
    }
#else
    if (this->frames++ <= 1)
    {
        this->first_pts = packet->pts + this->frames;
        this->first_dts = packet->dts + this->frames;
    }
#endif

    AVPacket *outPacket = av_packet_clone(packet);
    outPacket->pts = av_rescale_q_rnd(outPacket->pts - first_pts, this->i_video_stream->time_base, this->o_video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    outPacket->dts = av_rescale_q_rnd(outPacket->dts - first_dts, this->i_video_stream->time_base, this->o_video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    outPacket->duration = av_rescale_q(outPacket->duration, this->i_video_stream->time_base, this->o_video_stream->time_base);
    outPacket->pos = -1;
    //av_interleaved_write_frame(o_fmt_ctx, outPacket);
    av_write_frame(o_fmt_ctx, outPacket);

    this->duration = av_rescale_q(outPacket->pts, this->o_video_stream->time_base, {1, 1000}) / 1000.;

    unsigned long nMovements = movements ? movements->size() : 0;
    this->data << nMovements << endl;

    for (int i = 0; i < nMovements; i++)
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

                for (int i = 0; i < nMovements; i++)
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
