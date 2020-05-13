#include "AMQPNotifier.hpp"

const char *getKey(NotifyEvent event)
{
    switch (event)
    {
        case NotifyEvent::MOVEMENT:
            return "movement";

        case NotifyEvent::OBJECT:
            return "object";
    }

    throw std::runtime_error("Invalid notify event.");
}

AMQPNotifier::AMQPNotifier(std::string url)
{
    this->connection = new AMQP::TcpConnection(&handler, AMQP::Address(url));
    this->channel = new AMQP::TcpChannel(this->connection);

    channel->declareQueue(getKey(NotifyEvent::MOVEMENT), AMQP::durable).onError([](const char *message) {
        std::cerr << "Cannot declare queue \"movement\": " << message << std::endl;
    });

    channel->declareQueue(getKey(NotifyEvent::OBJECT), AMQP::durable).onError([](const char *message) {
        std::cerr << "Cannot declare queue \"movement\": " << message << std::endl;
    });
}

AMQPNotifier::~AMQPNotifier()
{
    delete this->channel;
    delete this->connection;
}

void AMQPNotifier::notify(std::string filename, NotifyEvent event)
{
    channel->publish("", getKey(event), filename);
    connection->process(this->handler.fd, this->handler.flags);
}