#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include "async_file_desc.hh"
#include "io_context.hh"
#include "file_read_operation.hh"
#include "task.hh"
#include "socket.hh"

class AsyncCommand : public AsyncFileDescriptor
{
public:
    /* Listen tcp non blocking socket */
    //AsyncCommand(std::string_view command, std::string_view arguments, IOContext& io_context);
        AsyncCommand(const AsyncCommand&) = delete;
    AsyncCommand(AsyncCommand&& command);
    //AsyncCommand(FILE * fdFromStream, Socket* socket);
    AsyncCommand(FILE * fdFromStream, AsyncFileDescriptor* socket);

    ~AsyncCommand();

    // std::task<std::shared_ptr<AsyncCommand>> accept();

    FileReadOperation recvfile(void* buffer, std::size_t len);

private:
    friend FileReadOperation;
    FILE * pipe= nullptr;
    friend IOContext;
    explicit AsyncCommand(int fd, IOContext& io_context);

};
