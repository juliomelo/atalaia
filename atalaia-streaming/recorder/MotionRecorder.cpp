#include "MotionRecorder.hpp"
#include "MovementDetector.hpp"
#include <list>
#include <iostream>
#include <opencv2/highgui.hpp>

using namespace std;

int Record::sequence = 0;

MotionRecorder::MotionRecorder(VideoStream *stream)
{
    shutdown = false;
    this->stream = stream;
    thread = new std::thread(threadProcess, this);
}

MotionRecorder::~MotionRecorder()
{
    shutdown = true;
    delete this->thread;
}

void MotionRecorder::threadProcess(MotionRecorder *recorder)
{
    MovementDetector movementDetector;
    std::list<AVPacket *> packetsFromKeyFrame;
    Record *record = NULL;
    int64_t dontStopUntil = 0;
    int64_t lastKeyFrame = 0;
    std::list<AVPacket *> waitingMovement;

    Record *teste = NULL;

    do
    {
        FrameQueueItem *item = recorder->stream->getQueue()->pop();

        if (!item)
        {
           recorder->shutdown = true;
           break;
        }

        // imshow("thread", item->mat);
        // waitKey(25);

        DetectedMovements movements = item->mat.empty() ? DetectedMovements() : movementDetector.detectMovement(item->mat);

        if (item->packet->flags & AV_PKT_FLAG_KEY) // Keyframe
        {
            for (list<AVPacket *>::iterator it = packetsFromKeyFrame.begin(); it != packetsFromKeyFrame.end(); ++it)
                av_packet_unref(*it);

            packetsFromKeyFrame.clear();
            lastKeyFrame = item->packet->pts;
        }

        if (!movements.empty())
        {
            // for (int i = 0; i < movements.size(); i++)
            // {
            //     polylines(item->mat, movements[i].contour, true, Scalar(0, 0, 255));
            // }

            // imshow("movements", item->mat);
            // waitKey(25);
            dontStopUntil = item->packet->pts + 5 / (item->time_base.num / (float)item->time_base.den);

            if (!record)
            {
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

        packetsFromKeyFrame.push_back(av_packet_clone(item->packet));

        if (!item->mat.empty() && movements.empty() && item->packet->pts >= dontStopUntil)
        {
            delete record;
            record = NULL;
            waitingMovement.clear();
        }
        else if (record && movements.empty())
            waitingMovement.push_back(av_packet_clone(item->packet));

        delete item;
    } while (!recorder->shutdown);

    if (record)
        delete record;

    if (teste)
        delete teste;
}

Record::Record(AVStream *i_video_stream)
{
    this->i_video_stream = i_video_stream;
    std::ostringstream ss;

    ss << "data/repo/" << sequence++ << ".atalaia";
    filename = ss.str();

    cout << "New file: " << filename << "\n";

    string video = filename + ".mp4";
    string data = filename + ".data";

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
    fwrite(&packet->size, sizeof(int), 1, this->data);
    fwrite(&packet->pts, sizeof(int64_t), 1, this->data);

    AVPacket *outPacket = av_packet_clone(packet);
    outPacket->pts = av_rescale_q_rnd(outPacket->pts, this->i_video_stream->time_base, this->o_video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    outPacket->dts = av_rescale_q_rnd(outPacket->dts, this->i_video_stream->time_base, this->o_video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    outPacket->duration = av_rescale_q(outPacket->duration, this->i_video_stream->time_base, this->o_video_stream->time_base);
    outPacket->pos = -1;
    av_interleaved_write_frame(o_fmt_ctx, outPacket);

    if (movements)
    {
        unsigned long nMovements = movements->size();

        fwrite(&nMovements, sizeof(unsigned long), 1, this->data);

        for (int i = 0; i < nMovements; i++)
        {
            unsigned long nPoints = (*movements)[i].contour.size();

            fwrite(&nPoints, sizeof(unsigned long), 1, this->data);

            for (unsigned long j = 0; j < nPoints; j++)
                fwrite(&((*movements)[i].contour[j]), sizeof(Point), 1, this->data);
        }
    }
}
