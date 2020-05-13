﻿// #include <stdio.h>
// #include <stdlib.h>
#include <iostream>
// #include <fstream>
// #include <sstream>
// #include <unistd.h>
// #include <sys/time.h>
// #include "MovementDetector.hpp"
#include "util/BlockingQueue.hpp"
#include "recorder/VideoStream.hpp"
#include "recorder/MotionRecorder.hpp"
#include "recorder/ObjectRecorder.hpp"
#include <thread>
#include "notify/amqp/AMQPNotifier.hpp"

extern "C"
{
#include <libavformat/avio.h>
}

using namespace cv;
using namespace std;

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("ERROR: URL not provided.\n");
		return EXIT_FAILURE;
	}

	if (argc == 3 && !strcmp(argv[1], "obj"))
	{
		string file = string(argv[2]);

		cout << "Analisando " << file << "\n";
		ObjectRecorder recorder;
		recorder.process(file);

		return EXIT_SUCCESS;
	}

	// Register everything
	//av_log_set_level( AV_LOG_DEBUG );
	av_register_all();
	//	avcodec_register_all();
	//avformat_network_init();

	ObjectRecorder objRecorder;
	VideoStreamQueue *queue[argc - 1];
	VideoStream *stream[argc - 1];
	MotionRecorder *recorder[argc - 1];
	//LocalNotifier notifier(&objRecorder);
	AMQPNotifier notifier("amqp://rabbitmq");

	for (int i = 0; i < argc - 1; i++)
	{
		queue[i] = new VideoStreamQueue(30 * 5);
		stream[i] = new VideoStream();
		stream[i]->start(argv[i + 1], queue[i]);
		recorder[i] = new MotionRecorder(stream[i], &notifier);
	}

	cout << "Waiting " << argc  << " streaming to finish.\n";

	for (int i = 0; i < argc - 1; i++)
		queue[i]->waitShutdown();

	cout << "Deleting..." << endl;
	
	for (int i = 0; i < argc - 1; i++)
	{
		delete recorder[i];
		delete stream[i];
		delete queue[i];
	}

	cout << "Wainting " << argc  << " object recorder to finish.\n";

	objRecorder.waitShutdown(true);

	cout << "Done.\n";

	return (EXIT_SUCCESS);
}

void processMotionRecord() {

}