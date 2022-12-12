#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#include "block_syscall.hh"

class AsyncCommand;

class FileReadOperation : public BlockSyscall<FileReadOperation, ssize_t>
{
public:
    FileReadOperation(AsyncCommand* socket, void* buffer, std::size_t len);
    ~FileReadOperation();

    ssize_t syscall();
    void suspend();
private:
    AsyncCommand* socket;
    void* buffer_;
    std::size_t len_;
};
