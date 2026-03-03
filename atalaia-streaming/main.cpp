#include "main.hpp"
#include "notify/amqp/AMQPNotifier.hpp"
#include "notify/LocalNotifier.hpp"
#include "notify/StdOutNotifier.hpp"
#include "recorder/MotionRecorder.hpp"
#include "recorder/ObjectRecorder.hpp"
#include "recorder/VideoStream.hpp"
#include "util/BlockingQueue.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libavformat/avio.h>
#include <signal.h>
#include <thread>

using namespace cv;
using namespace std;

typedef struct
{
	const char *command;
	int (* const function)(int, char **);
} t_commands;

int singleRecord(const char *url)
{
	VideoStreamQueue queue;
	VideoStream stream;
	
	stream.start(url, &queue);

	StdOutNotifier notifier(&stream, true);
	MotionRecorder recorder(&stream, &notifier, 10, true);

	queue.waitShutdown();

	return notifier.done ? 0 : -1;
}

int passiveRecord(const char *url)
{
	VideoStreamQueue queue;
	VideoStream stream;
	
	stream.start(url, &queue);

	StdOutNotifier notifier(&stream, false);
	MotionRecorder recorder(&stream, &notifier, 10, false);
	recorder.passive = true;

	// Read from stdin and set recorder->notifyThis = true when stdin receives a 'movement' command.
	std::thread stdin_thread([&recorder]() {
		std::string line;
		while (!terminating && std::getline(std::cin, line)) {
			if (line == "movement") {
#ifdef VERBOSE
				std::cout << "Received 'movement' command from stdin." << std::endl;
#endif
				recorder.notifyThis = true;
			}
		}
	});

	queue.waitShutdown();
	terminating = true;

	return notifier.done ? 0 : -1;
}

int monitorMovements(int argc, char **argv)
{
	if (argc < 2 || (argc == 2 && (strcmp(argv[1], "-s") || strcmp(argv[1], "-p"))))
	{
		cerr << "Syntax: atalaia-streaming movements <amqp url> <video url> [...video url]" << endl;
		cerr << "        atalaia-streaming movements <amqp url> -f <urls file>" << endl;
		cerr << "        atalaia-streaming movements <ampq url> -e <environment variable>" << endl;
		cerr << endl;
		cerr << "Single-file monitoring:" << endl;
		cerr << "        atalaia-streaming movements -s <video url>" << endl;
		cerr << endl;
		cerr << "Passive monitoring:" << endl;
		cerr << "        atalaia-streaming movements -p <video url>" << endl;
		return -1;
	}

	list<VideoStream *> streams;

	if (argc == 3 && strcmp(argv[1], "-f") == 0)
	{
#ifdef VERBOSE
		cout << "Monitoring URL's from file " << argv[2] << endl;
#endif
		ifstream urls(argv[2]);
		string url;

		while (getline(urls, url))
		{
			VideoStream *stream = new VideoStream();
			streams.push_back(stream);
			stream->start(url, new VideoStreamQueue());
		}
	}
	else if (argc == 3 && strcmp(argv[1], "-e") == 0)
	{
		const char *var = std::getenv(argv[2]);

#ifdef VERBOSE
		cout << "Using environment variable " << argv[2] << ": " << var << endl;
#endif
		char *value = strdup(var);

		for (char *url = strsep(&value, " "); url; url = strsep(&value, " "))
		{
			VideoStream *stream = new VideoStream();
			streams.push_back(stream);
			stream->start(url, new VideoStreamQueue());
		}
	}
	else if (argc == 2 && strcmp(argv[0], "-s") == 0)
	{
#ifdef VERBOSE
		cout << "Monitoring single stream" << endl;
#endif
		return singleRecord(argv[1]);
	}
	else if (argc == 2 && strcmp(argv[0], "-p") == 0)
	{
#ifdef VERBOSE
		cout << "Monitoring passive stream" << endl;
#endif
		return passiveRecord(argv[1]);
	}
	else if (argc >= 2)
		for (int i = 1; i < argc; i++)
		{
			VideoStream *stream = new VideoStream();
			streams.push_back(stream);
			stream->start(argv[i], new VideoStreamQueue());
		}

#ifdef VERBOSE
	cout << "Monitoring " << streams.size() << " video streams" << endl;
#endif

	AMQPNotifier notifier(argv[0]);
	notifier.getHandler()->read();

	list<MotionRecorder *> recorders;

	for (list<VideoStream *>::iterator it = streams.begin(); it != streams.end(); ++it)
		recorders.push_back(new MotionRecorder(*it, &notifier));

	bool failure = false;

	while (!terminating)
	{
		notifier.getConnection()->heartbeat();
		notifier.getHandler()->read();

		for (list<VideoStream *>::iterator it = streams.begin(); it != streams.end(); it++)
			switch ((*it)->getState())
			{
				case VideoState::FAILURE:
					failure = true;
					// yeah, proceed to next case switch

				case VideoState::SHUTDOWN:
#ifdef VERBOSE
					cout << "VideoState is " << (*it)->getState() << endl;
#endif
					terminating = true;
					break;
			}
	}

	for (list<VideoStream *>::iterator it = streams.begin(); it != streams.end(); ++it)
		(*it)->getQueue()->waitShutdown();

	for (list<VideoStream *>::iterator it = streams.begin(); it != streams.end(); ++it)
	{
		delete (*it)->getQueue();
		delete *it;
	}

	for (list<MotionRecorder *>::iterator it = recorders.begin(); it != recorders.end(); ++it)
		delete *it;

	return failure ? -1 : 0;
}

int monitorObjects(int argc, char **argv)
{
	if (argc != 1)
	{
		cerr << "Syntax: atalaia-streaming objects <amqp url|file path>" << endl;
		return -1;
	}

	if (strncmp(argv[0], "amqp://", 7) == 0)
	{
#ifdef VERBOSE
		cout << "Connecting to " << argv[0] << endl;
#endif

		AMQPNotifier notifier(argv[0]);
		ObjectRecorder recorder(&notifier);

		notifier.getChannel()->consume("movement").onReceived([&notifier, &recorder](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) {
			string file(message.body(), message.bodySize());

#ifdef VERBOSE
			cout << "Received message: " << file << endl;
#endif

			if (std::filesystem::exists(file + ".mp4") && std::filesystem::exists(file + ".movements"))
				recorder.process(file);
			else
				cerr << "File does not exist: " << file << endl;

			if (!terminating)
				notifier.getChannel()->ack(deliveryTag);
		});

		while (!terminating && true)
		{
			notifier.getConnection()->heartbeat();
			notifier.getHandler()->read();
		}
	}
	else
	{
#ifdef VERBOSE
		cout << "Processing " << argv[0] << endl;
#endif
		LocalNotifier notifier;
		ObjectRecorder recorder(&notifier);
		recorder.process(argv[0]);
	}

	return 0;
}

volatile sig_atomic_t terminating = 0;

void terminate(int signum)
{
	(void)signum;
	terminating = 1;
}

int main(int argc, char **argv)
{
	static t_commands commands[] = {
		{ "movements", monitorMovements },
		{ "objects", monitorObjects },
		NULL
	};

	if (argc < 2) {
		cerr << "Syntax: atalaia-streaming <command> [args]" << endl;
		return EXIT_FAILURE;
	}

	struct sigaction sigActionData;
	memset(&sigActionData, 0, sizeof(struct sigaction));
	sigActionData.sa_handler = terminate;

	sigaction(SIGTERM, &sigActionData, NULL);
	sigaction(SIGINT, &sigActionData, NULL);
	sigaction(SIGQUIT, &sigActionData, NULL);
	sigaction(SIGHUP, &sigActionData, NULL);

	//av_log_set_level( AV_LOG_DEBUG );
	av_register_all();

	if (argc > 2)
	{
		for (t_commands *cmd = commands; cmd; cmd++)
			if (strcmp(cmd->command, argv[1]) == 0)
				return cmd->function(argc - 2, argv + 2);

		cerr << "Unknown command: " << argv[1];
	}

#ifdef VERBOSE
	cout << "Local processing: " << argv[1] << endl;
#endif

	VideoStreamQueue *queue[argc - 1];
	VideoStream *stream[argc - 1];
	MotionRecorder *recorder[argc - 1];
	LocalNotifier notifier;
	
	for (int i = 0; i < argc - 1; i++)
	{
		queue[i] = new VideoStreamQueue(30 * 5);
		stream[i] = new VideoStream();
		stream[i]->start(argv[i + 1], queue[i]);
		recorder[i] = new MotionRecorder(stream[i], &notifier);
	}

	cout << "Waiting " << argc - 1  << " streaming to finish.\n";

	for (int i = 0; i < argc - 1; i++)
		queue[i]->waitShutdown();

	cout << "Finishing..." << endl;
	
	for (int i = 0; i < argc - 1; i++)
	{
		delete recorder[i];
		delete stream[i];
		delete queue[i];
	}

	cout << "Wainting " << argc  << " object recorder to finish.\n";

	notifier.waitShutdown();

	cout << "Done.\n";

	return (EXIT_SUCCESS);
}
