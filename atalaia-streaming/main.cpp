// #include <stdio.h>
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

#include <fstream>

using namespace cv;
using namespace std;

typedef struct
{
	const char *command;
	int (* const function)(int, char **);
} t_commands;

int monitorMovements(int argc, char **argv)
{
	if (argc < 2)
	{
		cerr << "Sintax: atalaia-streaming movements <amqp url> <video url> [...video url]" << endl;
		return -1;
	}

	AMQPNotifier notifier(argv[0]);
	list<VideoStream *> streams;

	if (argc == 3 && strcmp(argv[1], "-f") == 0)
	{
		cout << "Monitoring URL's from file " << argv[2] << endl;
		ifstream urls(argv[2]);
		string url;

		while (getline(urls, url))
		{
			VideoStream *stream = new VideoStream();
			streams.push_back(stream);
			stream->start(url, new VideoStreamQueue());
		}
	}
	else if (argc >= 2)
		for (int i = 1; i < argc; i++)
		{
			VideoStream *stream = new VideoStream();
			streams.push_back(stream);
			stream->start(argv[i], new VideoStreamQueue());
		}

	cout << "Monitoring " << streams.size() << " video streams" << endl;

	list<MotionRecorder *> recorders;

	for (list<VideoStream *>::iterator it = streams.begin(); it != streams.end(); ++it)
		recorders.push_back(new MotionRecorder(*it, &notifier));

	for (list<VideoStream *>::iterator it = streams.begin(); it != streams.end(); ++it)
		(*it)->getQueue()->waitShutdown();

	for (list<VideoStream *>::iterator it = streams.begin(); it != streams.end(); ++it)
	{
		delete (*it)->getQueue();
		delete *it;
	}

	for (list<MotionRecorder *>::iterator it = recorders.begin(); it != recorders.end(); ++it)
		delete *it;

	return 0;
}

int monitorObjects(int argc, char **argv)
{
	if (argc != 1)
	{
		cerr << "Sintax: atalaia-streaming objects <amqp url|file path>" << endl;
		return -1;
	}

	if (strncmp(argv[0], "amqp://", 7) == 0)
	{
		cout << "Connecting to " << argv[0] << endl;
		
		AMQPNotifier notifier(argv[0]);
		ObjectRecorder recorder(&notifier);

		notifier.getChannel()->consume("movement").onReceived([&notifier, &recorder](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) {
			string file(message.body(), message.bodySize());
			cout << "Received message: " << file << endl;
			recorder.process(file);
			notifier.getChannel()->ack(deliveryTag);
		});

		while (true)
			notifier.getHandler()->read();
	}
	else
	{
		cout << "Processing " << argv[0] << endl;
		LocalNotifier notifier(NULL);
		ObjectRecorder recorder(&notifier);
		recorder.process(argv[0]);
	}

	return 0;
}

int main(int argc, char **argv)
{
	static t_commands commands[] = {
		{ "movements", monitorMovements },
		{ "objects", monitorObjects },
		NULL
	};

	if (argc < 2) {
		cerr << "Sintax: atalaia-streaming <command> [args]" << endl;
		return EXIT_FAILURE;
	}

	//av_log_set_level( AV_LOG_DEBUG );
	av_register_all();

	if (argc > 2)
	{
		for (t_commands *cmd = commands; cmd; cmd++)
			if (strcmp(cmd->command, argv[1]) == 0)
				return cmd->function(argc - 2, argv + 2);

		cerr << "Unknown command: " << argv[1];
	}

	cout << "Local processing: " << argv[1] << endl;

	ObjectRecorder objRecorder(NULL);
	VideoStreamQueue *queue[argc - 1];
	VideoStream *stream[argc - 1];
	MotionRecorder *recorder[argc - 1];
	LocalNotifier notifier(&objRecorder);
	
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
