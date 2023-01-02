#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include "async_file_desc.hh"
#include "io_context.hh"
#include "file_read_operation.hh"
#include "file_write_operation.hh"
#include "task.hh"
#include "socket.hh"


/// @brief PipedAsyncCommand implementation using popen or pipe fork excec
class PipedAsyncCommand
{
public:
    /* Listen tcp non blocking socket */
    PipedAsyncCommand(const PipedAsyncCommand&) = delete;
    PipedAsyncCommand(std::string cmd, AsyncFileDescriptor* socket);
    ~PipedAsyncCommand();

    FileReadOperation readpipe(void* buffer, std::size_t len);
    FileWriteOperation writepipe(void* buffer, std::size_t len);

private:
    friend FileReadOperation;
    friend AsyncFileDescriptor;
    friend Socket;
    //      parent        child
    //      fd1[1]        fd1[0]
    //        4 -- fd_W --> 3 
    //      fd2[0]        fd2[1]
    //        5 <-- fd_r -- 6 
    int fd_w[2];
    int fd_r[2]; 
    
    AsyncFileDescriptor *async_read_end_fd;
    AsyncFileDescriptor *async_write_end_fd;

};

