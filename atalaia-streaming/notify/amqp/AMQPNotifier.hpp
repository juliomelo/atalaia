#pragma once

#include <amqpcpp.h>
#include <amqpcpp/linux_tcp.h>
#include "../Notify.hpp"
#include "AtalaiaTcpHandler.hpp"
#include "AMQPNotifier.hpp"

class AMQPNotifier : public Notifier
{
    public:
        AMQPNotifier(std::string url);
        virtual ~AMQPNotifier();

        virtual void notify(string filename, NotifyEvent event);

    private:
        AMQP::TcpConnection *connection;
        AMQP::TcpChannel *channel;
        AtalaiaTcpHandler handler;
};
