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

long long current_timestamp()
{
	struct timeval te;
	gettimeofday(&te, NULL);										 // get current time
	long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds
	// printf("milliseconds: %lld\n", milliseconds);
	return milliseconds;
}

VideoStream::VideoStream()
{
    this->thread = NULL;
    this->queue = NULL;
    this->threadState = WAITING;
    this->format_ctx = NULL;
}

int VideoStream::start(std::string url, BlockingQueue<FrameQueueItem *> *queue)
{
    if (this->thread != NULL)
        return EXIT_FAILURE;

    this->queue = queue;
    this->url = url;
    this->thread = new std::thread(this->threadProcess, this);

    return 0;
}

void VideoStream::threadProcess(VideoStream *data)
{
    cout << "Connecting to " << data->url << "...\n";

    data->threadState = PROCESSING;
    AVDictionary *opts = NULL;

    if (!strncmp(data->url.c_str(), "rtsp://", 7))
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);

    if (avformat_open_input(&data->format_ctx, data->url.c_str(), NULL, &opts) != 0)
    {
        cout << "Can't open " << data->url << "\n";

        if (!opts || avformat_open_input(&data->format_ctx, data->url.c_str(), NULL, NULL) != 0)
        {
            data->threadState = FAILURE;
            data->thread = NULL;
            data->queue->close();
            return;
        }

        cout << "Connected without TCP: " << data->url << "\n";
    }

    if (!data->format_ctx)
    {
        cerr << "WHAT?! No format_ctx!\n";
        data->threadState = FAILURE;
        data->thread = NULL;
        data->queue->close();
        return;
    }

    if (opts) {
        av_dict_free(&opts);
    }

    if (avformat_find_stream_info(data->format_ctx, NULL) < 0)
    {
        data->threadState = FAILURE;
        data->thread = NULL;
        data->queue->close();
        return;
    }

    AVCodec *vcodec = nullptr;

    int VideoStream_index = av_find_best_stream(data->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);

    if (VideoStream_index < 0)
    {
        data->threadState = FAILURE;
        data->thread = NULL;
        data->queue->close();
        return;
    }

    AVStream *vstrm = data->vstrm = data->format_ctx->streams[VideoStream_index];

    if (avcodec_open2(vstrm->codec, vcodec, nullptr) < 0)
    {
        data->threadState = FAILURE;
        data->thread = NULL;
        data->queue->close();
        return;
    }

    std::cout
        << "url: " << data->url << "\n"
        << "format: " << data->format_ctx->iformat->name << "\n"
        << "vcodec: " << vcodec->name << "\n"
        << "size:   " << vstrm->codec->width << 'x' << vstrm->codec->height << "\n"
        << "fps:    " << av_q2d(vstrm->codec->framerate) << " [fps]\n"
        << "length: " << av_rescale_q(vstrm->duration, vstrm->time_base, {1, 1000}) / 1000. << " [sec]\n"
        //		<< "pixfmt: " << av_get_pix_fmt_name(vstrm->codec->pix_fmt) << "\n"
        << "frame:  " << vstrm->nb_frames << "\n"
        << std::flush;

    av_read_play(data->format_ctx); //play RTSP

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
        data->thread = NULL;
        data->queue->close();
        return;
    }

    // allocate frame buffer for output
    AVFrame *frame = av_frame_alloc();
    data->framebuf = vector<uint8_t>(avpicture_get_size(dst_pix_fmt, dst_width, dst_height));
    avpicture_fill(reinterpret_cast<AVPicture *>(frame), data->framebuf.data(), dst_pix_fmt, dst_width, dst_height);

    AVFrame *decframe = av_frame_alloc();
    long long start = current_timestamp();
    long long now;
    uint frames = 0;

    AVPacket packet;
    av_init_packet(&packet);

    while (!terminating && data->threadState == PROCESSING && av_read_frame(data->format_ctx, &packet) >= 0)
    {
        now = current_timestamp();

        if (packet.stream_index == VideoStream_index)
        {
            // packet is video
            int got_pic = 0;

            avcodec_decode_video2(vstrm->codec, decframe, &got_pic, &packet);

            if (got_pic)
            {
                sws_scale(swsctx, decframe->data, decframe->linesize, 0, decframe->height, frame->data, frame->linesize);
                cv::Mat img(dst_height, dst_width, CV_8UC3, data->framebuf.data(), frame->linesize[0]);

                if (!img.empty())
                {
                    FrameQueueItem *item = new FrameQueueItem;
                    item->packet = av_packet_clone(&packet);
                    item->mat = img.clone();
                    item->time_base = vstrm->time_base;

                    // if (!strncmp(data->url.c_str(), "rtsp://", 7))
                    // {
                    //     cv::resize(img, img, Size(320, 240));
                    //     imshow(data->url.c_str(), img);
                    //     waitKey(100);
                    // }

                    if (!data->queue->try_push(item)) {
                        cout << "Can't push item to queue!\n";
                        delete item;
                    }
                }
            }
            else
            {
                FrameQueueItem *item = new FrameQueueItem;
                item->packet = av_packet_clone(&packet);
                item->time_base = vstrm->time_base;

                if (!data->queue->try_push(item))
                {
                    cout << "Can't push item to queue!\n";
                    delete item;
                }
            }
        }

        av_packet_unref(&packet);
        av_init_packet(&packet);

        long long ellapsed = now - start;
        frames++;

        if (ellapsed >= 1000 * 15)
        {
            printf("FPS: %f\n", frames / (ellapsed / (float)1000) * 4);
            frames = 0;
            start = now;
        }
    }

    cout << "Stopping " << data->url << "...\n";

    av_frame_free(&frame);
    av_frame_free(&decframe);

    //av_free(vcodec);
    //av_free(data->format_ctx);
    sws_freeContext(swsctx);


    data->threadState = SHUTDOWN;
    data->thread = NULL;

    data->queue->close();
    data->queue->waitShutdown();


    if (data->format_ctx)
    {
        avformat_close_input(&data->format_ctx);
        data->format_ctx = NULL;
    }
}

VideoStream::~VideoStream()
{
    cout << "VideoStream Shutdown\n";

    if (this->thread)
    {
        this->threadState = SHUTDOWN;
        this->thread->join();
        delete this->thread;
    }
}

FrameQueueItem::~FrameQueueItem()
{
    av_packet_unref(this->packet);
}