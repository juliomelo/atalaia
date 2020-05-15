#pragma once

#include <amqpcpp.h>
#include <amqpcpp/linux_tcp.h>
#include "../Notify.hpp"
#include "AtalaiaTcpHandler.hpp"
#include "AMQPNotifier.hpp"
#include <set>

class AMQPNotifier : public Notifier
{
    public:
        AMQPNotifier(std::string url);
        virtual ~AMQPNotifier();
        
        virtual void notify(string filename, NotifyEvent event, std::string arg = "");
        inline AMQP::TcpChannel *getChannel() { return this->channel; }
        inline AtalaiaTcpHandler *getHandler() { return &this->handler; }

    private:
        AMQP::TcpConnection *connection;
        AMQP::TcpChannel *channel;
        AtalaiaTcpHandler handler;
        std::set<string> objectTypes;
};
