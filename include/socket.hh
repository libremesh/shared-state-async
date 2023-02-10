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
//#include "task.hpp"

class Socket : public AsyncFileDescriptor
{
public:
    /* Listen tcp non blocking socket */
    Socket(std::string_view port, IOContext& io_context);
    Socket(const Socket&) = delete;
    Socket(Socket&& socket);

    ~Socket();

    std::task<std::unique_ptr<Socket>> accept();

    SocketRecvOperation recv(void* buffer, std::size_t len);
    SocketSendOperation send(void* buffer, std::size_t len);
    explicit Socket(int fd, IOContext& io_context);
private:
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    friend FileReadOperation;
    FILE * pipe= nullptr;
    friend IOContext;



};
