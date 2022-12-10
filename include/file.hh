#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

#include "io_context.hh"
#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"
#include "task.hh"

class File
{
public:
    File(FILE * filepointer, IOContext& io_context);
    //File(const Socket&) = delete;
    //File(Socket&& socket);

    ~File();

    //std::task<std::shared_ptr<File>> accept();

    FileRecvOperation recv(void* buffer, std::size_t len);
    //SocketSendOperation send(void* buffer, std::size_t len);

    bool resumeRecv()
    {
        if (!coroRecv_)
            return false;
        coroRecv_.resume();
        return true;
    }

    /*bool resumeSend()
    {
        if (!coroSend_)
            return false;
        coroSend_.resume();
        return true;
    }*/

private:
    friend FileRecvOperation;
    IOContext& io_context_;
    int fd_ = -1;
    friend IOContext;
    uint32_t io_state_ = 0;
    uint32_t io_new_state_ = 0;

    explicit Socket(int fd, IOContext& io_context);
    std::coroutine_handle<> coroRecv_;
};
