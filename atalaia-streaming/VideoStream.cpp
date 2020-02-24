#include "VideoStream.hpp"
#include "MovementDetector.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/time.h>
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"

CircularPacketArray::CircularPacketArray(AVRational frame_rate)
{
    double fps = av_q2d(frame_rate);

    if (isnan(fps))
        fps = 30;

    this->buffer_size = (int)fps * BUFFER_SECONDS;
    this->full = false;

    cout << "Buffer size: " << this->buffer_size << "\n";

    this->packets = new AVPacket[this->buffer_size];
    this->currentIndex = 0;

    for (int i = 0; i < this->buffer_size; i++)
        av_init_packet(&packets[i]);

}

CircularPacketArray::CircularPacketArray(int buffer_size)
{
    this->buffer_size = buffer_size;
    this->full = false;

    this->packets = new AVPacket[this->buffer_size];
    this->currentIndex = 0;

    for (int i = 0; i < this->buffer_size; i++)
        av_init_packet(&packets[i]);
}

CircularPacketArray::~CircularPacketArray()
{
    for (int i = 0; i < this->buffer_size; i++)
        av_packet_unref(&this->packets[i]);

    delete this->packets;
}

void CircularPacketArray::next()
{
    currentIndex = (currentIndex + 1) % this->buffer_size;
    av_packet_unref(&this->packets[currentIndex]);
    av_init_packet(&packets[currentIndex]);
}

AVPacket *CircularPacketArray::current()
{
    return &packets[currentIndex];
}

vector<AVPacket> CircularPacketArray::getPacketsSince(int64_t pts)
{
    vector<AVPacket> packets;

    for (int i = this->full ? (currentIndex + 1) % this->buffer_size : 0; i != currentIndex; i = (i + 1) % this->buffer_size)
    {
        AVPacket copy;
        av_init_packet(&copy);
        av_packet_ref(&copy, &this->packets[i]);

        if (copy.pts >= pts)
            packets.push_back(copy);
        else
            av_packet_unref(&copy);
    }

    return packets;
}

int CircularPacketArray::size()
{
    return this->buffer_size;
}

long long current_timestamp()
{
	struct timeval te;
	gettimeofday(&te, NULL);										 // get current time
	long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds
	// printf("milliseconds: %lld\n", milliseconds);
	return milliseconds;
}

VideoStream::VideoStream(BlockingQueue<FrameQueueItem *> *queue)
{
    this->thread = NULL;
    this->queue = queue;
    this->threadState = WAITING;
    this->packets = NULL;
}

int VideoStream::start(std::string url)
{
    if (this->thread != NULL)
        return EXIT_FAILURE;

    this->url = url;
    this->thread = new std::thread(this->threadProcess, this);

    return 0;
}

void VideoStream::threadProcess(VideoStream *data)
{
    cout << "Connecting to " << data->url << "...\n";

    data->threadState = PROCESSING;
    AVFormatContext *format_ctx = NULL;

    if (avformat_open_input(&format_ctx, data->url.c_str(), NULL, NULL) != 0)
    {
        data->threadState = FAILURE;
        return;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0)
    {
        data->threadState = FAILURE;
        return;
    }

    AVCodec *vcodec = nullptr;

    int VideoStream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);

    if (VideoStream_index < 0)
    {
        data->threadState = FAILURE;
        return;
    }

    AVStream *vstrm = format_ctx->streams[VideoStream_index];

    if (avcodec_open2(vstrm->codec, vcodec, nullptr) < 0)
    {
        data->threadState = FAILURE;
        return;
    }

    std::cout
        << "url: " << data->url << "\n"
        << "format: " << format_ctx->iformat->name << "\n"
        << "vcodec: " << vcodec->name << "\n"
        << "size:   " << vstrm->codec->width << 'x' << vstrm->codec->height << "\n"
        << "fps:    " << av_q2d(vstrm->codec->framerate) << " [fps]\n"
        << "length: " << av_rescale_q(vstrm->duration, vstrm->time_base, {1, 1000}) / 1000. << " [sec]\n"
        //		<< "pixfmt: " << av_get_pix_fmt_name(vstrm->codec->pix_fmt) << "\n"
        << "frame:  " << vstrm->nb_frames << "\n"
        << std::flush;

    if (data->packets)
        delete data->packets;

    data->packets = new CircularPacketArray(vstrm->codec->framerate);

    av_read_play(format_ctx); //play RTSP

    const int dst_width = vstrm->codec->width;
    const int dst_height = vstrm->codec->height;
    const AVPixelFormat dst_pix_fmt = AV_PIX_FMT_BGR24;

    SwsContext *swsctx = sws_getCachedContext(
        nullptr, vstrm->codec->width, vstrm->codec->height, vstrm->codec->pix_fmt,
        dst_width, dst_height, dst_pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);

    if (!swsctx)
    {
        std::cerr << "fail to sws_getCachedContext";
        data->threadState = FAILURE;
        return;
    }

    // allocate frame buffer for output
    AVFrame *frame = av_frame_alloc();
    std::vector<uint8_t> framebuf(avpicture_get_size(dst_pix_fmt, dst_width, dst_height));
    avpicture_fill(reinterpret_cast<AVPicture *>(frame), framebuf.data(), dst_pix_fmt, dst_width, dst_height);

    AVFrame *decframe = av_frame_alloc();
    long long start = current_timestamp();
    long long now;
    uint frames = 0;

    MovementDetector movementDetector;

    while ((data->threadState == PROCESSING || data->threadState == RECORDING) && av_read_frame(format_ctx, data->packets->current()) >= 0)
    {
        now = current_timestamp();

        if (data->packets->current()->stream_index == VideoStream_index)
        {
            // packet is video
            int got_pic = 0;

            avcodec_decode_video2(vstrm->codec, decframe, &got_pic, data->packets->current());

            if (got_pic)
            {
                sws_scale(swsctx, decframe->data, decframe->linesize, 0, decframe->height, frame->data, frame->linesize);
                cv::Mat img(dst_height, dst_width, CV_8UC3, framebuf.data(), frame->linesize[0]);

                if (!img.empty())
                {
                    DetectedMovements movements = movementDetector.detectMovement(img);

                    if (data->threadState == RECORDING || !movements.empty())
                    {
                        FrameQueueItem *item = new FrameQueueItem;
                        item->packet = av_packet_clone(data->packets->current());
                        item->mat = img.clone();
                        item->movements = movements;
                        item->time_base = vstrm->time_base;

                        if (!data->queue->try_push(item)) {
                            cout << "Can't push item to queue!\n";
                            av_free_packet(item->packet);
                            delete item;
                        }
                    }
                }
            }
        }

        data->packets->next();

        long long ellapsed = now - start;
        frames++;

        if (ellapsed >= 1000 * 60)
        {
            printf("FPS: %f\n", frames / (ellapsed / (float)1000));
            frames = 0;
            start = now;
        }
    }

    printf("Stopping...");

    av_read_pause(format_ctx);
    av_frame_free(&decframe);

    if (data->threadState != SHUTDOWN)
        data->threadState = WAITING;
    else
        delete data->packets;

    data->thread = NULL;
}

std::vector<AVPacket> VideoStream::getPacketsSince(int64_t pts)
{
    return this->packets->getPacketsSince(pts);
}

VideoStream::~VideoStream()
{
    if (this->thread)
        this->threadState = SHUTDOWN;
    else
        delete this->packets;
}

FrameQueueItem::~FrameQueueItem()
{
    av_free_packet(this->packet);
}