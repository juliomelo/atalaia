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
        exit(-1);
    });

    channel->declareExchange(getKey(NotifyEvent::OBJECT), AMQP::ExchangeType::direct).onError([](const char *message) {
        std::cerr << "Cannot declare exchange \"object\": " << message << std::endl;
        exit(-1);
    });
}

AMQPNotifier::~AMQPNotifier()
{
    delete this->channel;
    delete this->connection;
}

void AMQPNotifier::notify(std::string filename, NotifyEvent event, std::string arg)
{
    std::string key = getKey(event);

    cout << "[+] Notify: " << filename << " " << key << " " << arg << endl;

    switch (event)
    {
        case NotifyEvent::MOVEMENT:
            channel->publish("", key, filename);
            break;

        case NotifyEvent::OBJECT:
            channel->publish(key, arg, filename);
            break;
    }

    if (!this->handler.reading)
        connection->process(this->handler.fd, this->handler.flags);
}