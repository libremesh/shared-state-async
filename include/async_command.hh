#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include "async_file_desc.hh"
#include "io_context.hh"
#include "pipe_file_read_operation.hh"
#include "task.hh"
#include "socket.hh"


/// @brief AsyncCommand implementation using popen or pipe fork excec
class AsyncCommand :AsyncFileDescriptor
{
public:
    /* Listen tcp non blocking socket */
    AsyncCommand(const AsyncCommand&) = delete;
    AsyncCommand(AsyncCommand&& command);
    AsyncCommand(FILE * fdFromStream, AsyncFileDescriptor* socket);
    AsyncCommand(std::string cmd, AsyncFileDescriptor* socket);
    ~AsyncCommand();

    PipeFileReadOperation recvfile(void* buffer, std::size_t len);


private:
    friend PipeFileReadOperation;
    FILE * pipe= nullptr;
    friend IOContext;
    explicit AsyncCommand(int fd, IOContext& io_context);

};
