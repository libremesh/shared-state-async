#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include "async_file_desc.hh"
#include "io_context.hh"
#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"
#include "file_read_operation.hh"
#include "task.hh"

class Socket :AsyncFileDescriptor
{
public:
    /* Listen tcp non blocking socket */
    Socket(std::string_view port, IOContext& io_context);
    Socket(const Socket&) = delete;
    Socket(FILE * fd, Socket* socket);
    Socket(Socket&& socket);

    ~Socket();

    std::task<std::shared_ptr<Socket>> accept();

    SocketRecvOperation recv(void* buffer, std::size_t len);
    SocketSendOperation send(void* buffer, std::size_t len);
    FileReadOperation recvfile(void* buffer, std::size_t len);


    bool resumeRecv()
    {
        if (!coroRecv_)
            return false;
        coroRecv_.resume();
        return true;
    }

    bool resumeSend()
    {
        if (!coroSend_)
            return false;
        coroSend_.resume();
        return true;
    }

private:
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    friend FileReadOperation;
 //   IOContext& io_context_;
    FILE * pipe= nullptr;
    friend IOContext;


    explicit Socket(int fd, IOContext& io_context);
    //std::coroutine_handle<> coroRecv_;
    //std::coroutine_handle<> coroSend_;
};
