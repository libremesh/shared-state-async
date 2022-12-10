#pragma once

#include <set>
#include <stdexcept>
#include <sys/epoll.h>

#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"
#include "file_read_operation.hh"

/* Just an epoll wrapper */
//This class will work as dispacher, freeing epool suspended corotasks
class Socket;

class IOContext
{
public:
    IOContext()
        : fd_{epoll_create1(0)}
    {
        if (fd_ == -1)
            throw std::runtime_error{"epoll_create1"};
            //todo: fix a esto no usaremos exept hacer un factory con un unique pointer que tire error
            //ver que pasa cuando falla 
    }

    void run();
private:
    constexpr static std::size_t max_events = 10;
    const int fd_;

    // Fill it by watchRead / watchWrite
    std::set<Socket*> processedSockets;

    friend Socket;
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    friend FileReadOperation;
    void attach(Socket* socket);
    void attachreadonly(Socket* socket);
    void watchRead(Socket* socket);
    void unwatchRead(Socket* socket);
    void watchWrite(Socket* socket);
    void unwatchWrite(Socket* socket);
    void detach(Socket* socket);
};
