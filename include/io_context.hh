#pragma once

#include <set>
#include <stdexcept>
#include <sys/epoll.h>

#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"
#include "file_read_operation.hh"
#include "file_write_operation.hh"
#include "pipe_file_read_operation.hh"

/* Just an epoll wrapper */
//This class will work as dispacher, freeing epool suspended corotasks
class Socket;
class AsyncFileDescriptor;
class AsyncCommand;
class PipedAsyncCommand;

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
    constexpr static std::size_t max_events = 20;
    const int fd_; //iocontext epool fd

    // Fill it by watchRead / watchWrite
    std::set<AsyncFileDescriptor*> processedSockets;

    friend AsyncFileDescriptor;
    friend Socket;
    friend AsyncCommand;
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    friend FileReadOperation;
    friend FileWriteOperation;
    friend PipeFileReadOperation;
    friend PipedAsyncCommand;
    void attach(AsyncFileDescriptor* socket);
    void attachReadonly(AsyncFileDescriptor* socket);
    void attachWriteOnly(AsyncFileDescriptor* socket);
    void watchRead(AsyncFileDescriptor* socket);
    void unwatchRead(AsyncFileDescriptor* socket);
    void watchWrite(AsyncFileDescriptor* socket);
    void unwatchWrite(AsyncFileDescriptor* socket);
    void detach(AsyncFileDescriptor* socket);
};
