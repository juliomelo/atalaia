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
#include <set>
#include "CameraAtalaia.hpp"

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

	// Register everything
	//av_log_set_level( AV_LOG_DEBUG );
	av_register_all();
	//	avcodec_register_all();
	//avformat_network_init();

	CameraAtalaia *streams[argc - 1];

	for (int i = 0; i < argc - 1; i++)
	{
		streams[i] = new CameraAtalaia(argv[i + 1]);
	}

	int wait;
	cin >> wait;

	return (EXIT_SUCCESS);
}
