#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#include "block_syscall.hh"

class AsyncCommand;

class PipeFileReadOperation : public BlockSyscall<PipeFileReadOperation, ssize_t>
{
public:
    PipeFileReadOperation(AsyncCommand* socket, void* buffer, std::size_t len);
    ~PipeFileReadOperation();

    ssize_t syscall();
    void suspend();
private:
    AsyncCommand* socket;
    void* buffer_;
    std::size_t len_;
};
