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

class CircularPacketArray
{
    private:
        int buffer_size;
        AVPacket *packets;
        int currentIndex;

    public:
        CircularPacketArray(AVRational frame_rate)
        {
            double fps = av_q2d(frame_rate);

            if (isnan(fps))
                fps = 30;

            this->buffer_size = (int)fps * BUFFER_SECONDS;

            cout << "Buffer size: " << this->buffer_size << "\n";

            this->packets = new AVPacket[this->buffer_size];
            this->currentIndex = 0;

            for (int i = 0; i < this->buffer_size; i++)
                av_init_packet(&packets[i]);
        }

        ~CircularPacketArray()
        {
            for (int i = 0; i < this->buffer_size; i++)
                av_free_packet(&this->packets[i]);

            delete this->packets;
        }

        void next()
        {
            currentIndex = (currentIndex + 1) % this->buffer_size;
            av_free_packet(&this->packets[currentIndex]);
            av_init_packet(&packets[currentIndex]);
        }

        AVPacket *current()
        {
            return &packets[currentIndex];
        }
};

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
}

int VideoStream::start(char *url)
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

    data->threadState = 1;
    AVFormatContext *format_ctx = NULL;

    if (avformat_open_input(&format_ctx, data->url, NULL, NULL) != 0)
    {
        data->threadState = EXIT_FAILURE;
        return;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0)
    {
        data->threadState = EXIT_FAILURE;
        return;
    }

    AVCodec *vcodec = nullptr;

    int VideoStream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);

    if (VideoStream_index < 0)
    {
        data->threadState = EXIT_FAILURE;
        return;
    }

    AVStream *vstrm = format_ctx->streams[VideoStream_index];

    if (avcodec_open2(vstrm->codec, vcodec, nullptr) < 0)
    {
        data->threadState = EXIT_FAILURE;
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

    CircularPacketArray packets(vstrm->codec->framerate);

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
        data->threadState = EXIT_FAILURE;
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

    while (data->threadState == 1 && av_read_frame(format_ctx, packets.current()) >= 0)
    {
        if (packets.current()->stream_index == VideoStream_index)
        {
            // packet is video
            int got_pic = 0;

            avcodec_decode_video2(vstrm->codec, decframe, &got_pic, packets.current());

            if (got_pic)
            {
                sws_scale(swsctx, decframe->data, decframe->linesize, 0, decframe->height, frame->data, frame->linesize);
                cv::Mat img(dst_height, dst_width, CV_8UC3, framebuf.data(), frame->linesize[0]);

                if (!img.empty())
                {
                    DetectedMovements movements = movementDetector.detectMovement(img);

                    if (!movements.empty())
                    {
                        FrameQueueItem *item = new FrameQueueItem;
                        item->packet = av_packet_clone(packets.current());
                        item->mat = img.clone();
                        item->movements = movements;

                        if (!data->queue->try_push(item)) {
                            cout << "Can't push item to queue!\n";
                            av_free_packet(item->packet);
                            delete item;
                        }
                    }
                }
            }
        }

        //av_free_packet(&packet);
        packets.next();

        now = current_timestamp();

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

    data->threadState = 0;
    data->thread = NULL;
}

VideoStream::~VideoStream()
{
    if (this->thread)
        this->threadState = 2;
}

FrameQueueItem::~FrameQueueItem()
{
    av_free_packet(this->packet);
}