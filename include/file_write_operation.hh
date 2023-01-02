#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#include "block_syscall.hh"

class AsyncFileDescriptor;

class FileWriteOperation : public BlockSyscall<FileWriteOperation, ssize_t>
{
public:
    FileWriteOperation(AsyncFileDescriptor* socket, void* buffer, std::size_t len);
    ~FileWriteOperation();

    ssize_t syscall();
    void suspend();
private:
    AsyncFileDescriptor* socket;
    void* buffer_;
    std::size_t len_;
};
