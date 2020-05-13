#include "AtalaiaTcpHandler.hpp"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

void AtalaiaTcpHandler::read()
{
    struct timeval tv
    {
        1, 0
    };

    FD_ZERO(&this->set);
    FD_SET(this->fd, &this->set);

    int result = select(FD_SETSIZE, &this->set, NULL, NULL, &tv);

    if ((result == -1) && errno == EINTR)
        std::cerr << "Socket error" << std::endl;
    else if (result > 0 && FD_ISSET(this->fd, &this->set))
        this->connection->process(this->fd, this->flags);
}