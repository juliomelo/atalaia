#pragma once

#include <amqpcpp.h>
#include <amqpcpp/linux_tcp.h>


class AtalaiaTcpHandler : public AMQP::TcpHandler
{
private:
    /**
    * Method that is called when the connection succeeded
    * @param socket Pointer to the socket
    */
    virtual void onConnected(AMQP::TcpConnection *connection)
    {
        std::cout << "connected" << std::endl;
    }

    /**
     *  When the connection ends up in an error state this method is called.
     *  This happens when data comes in that does not match the AMQP protocol
     *
     *  After this method is called, the connection no longer is in a valid
     *  state and can be used. In normal circumstances this method is not called.
     *
     *  @param  connection      The connection that entered the error state
     *  @param  message         Error message
     */
    virtual void onError(AMQP::TcpConnection *connection, const char *message)
    {
        // report error
        std::cerr << "AMQP TCPConnection error: " << message << std::endl;
        exit(-2);
    }

    /**
     *  Method that is called when the connection was closed.
     *  @param  connection      The connection that was closed and that is now unusable
     */
    virtual void onClosed(AMQP::TcpConnection *connection)
    {
        std::cout << "closed" << std::endl;
    }

    /**
     *  Method that is called by AMQP-CPP to register a filedescriptor for readability or writability
     *  @param  connection  The TCP connection object that is reporting
     *  @param  fd          The filedescriptor to be monitored
     *  @param  flags       Should the object be monitored for readability or writability?
     */
    virtual void monitor(AMQP::TcpConnection *connection, int fd, int flags)
    {
        std::cout << "amqp monitor - fd: " << fd << "; flags: " << flags << std::endl;

        this->connection = connection;

        if (flags == 0)
            return;

        if (flags & AMQP::readable)
        {
            FD_SET(fd, &set);
            this->fd = fd;
            this->flags = flags;
        }
    }

public:
    int fd = -1;
    int flags = 0;
    fd_set set;
    AMQP::TcpConnection *connection;
    bool reading;

    void read();
};