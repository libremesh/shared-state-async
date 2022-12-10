#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#include "block_syscall.hh"

class Socket;

class FileReadOperation : public BlockSyscall<FileReadOperation, ssize_t>
{
public:
    FileReadOperation(Socket* socket, void* buffer, std::size_t len);
    ~FileReadOperation();

    ssize_t syscall();
    void suspend();
private:
    Socket* socket;
    void* buffer_;
    std::size_t len_;
};
