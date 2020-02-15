// #include <stdio.h>
// #include <stdlib.h>
#include <iostream>
// #include <fstream>
// #include <sstream>
// #include <unistd.h>
// #include <sys/time.h>
// #include "MovementDetector.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "VideoStream.hpp"
#include "util/BlockingQueue.hpp"
#include "ObjectDetector.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
}

using namespace cv;
using namespace std;

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("ERROR: URL not provided.\n");
		return EXIT_FAILURE;
	}

	YoloV3ObjectDetector objectDetector;

	// Register everything
	//av_log_set_level( AV_LOG_DEBUG );
	av_register_all();
	//	avcodec_register_all();
	//avformat_network_init();

	VideoStream *streams[argc - 1];
	BlockingQueue<FrameQueueItem *> queue(30 * 5 * 10);

	for (int i = 0; i < argc - 1; i++)
	{
		streams[i] = new VideoStream(&queue);
		streams[i]->start(argv[i + 1]);
	}

	for (;;)
	{
		FrameQueueItem *frame = queue.pop();

		Mat segmented = frame->mat.clone();
		Scalar color( 0, 0, 255 );

		for (int i = 0; i < frame->movements.size(); i++)
    		polylines(segmented, frame->movements[i].contour, true, Scalar(0, 0, 255));

		imshow("movement", segmented);

		DetectedObjects objects = objectDetector.classifyFromMovements(frame->mat, frame->movements);

		if (!objects.empty())
		{
			for (int i = 0; i < objects.size(); i++) {
				objectDetector.drawObject(frame->mat, objects[i]);
			}

			imshow("objects", frame->mat);
			cv::waitKey((int)(1000.0 / 30));
		}

		delete frame;
	}

	return (EXIT_SUCCESS);
}
