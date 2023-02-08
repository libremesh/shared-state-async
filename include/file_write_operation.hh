#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include "block_syscall.hh"
#include <memory>

class AsyncFileDescriptor;

class FileWriteOperation : public BlockSyscall<FileWriteOperation, ssize_t>
{
public:
    FileWriteOperation(std::shared_ptr<AsyncFileDescriptor> socket, void* buffer, std::size_t len);
    ~FileWriteOperation();

    ssize_t syscall();
    void suspend();
private:
    std::shared_ptr<AsyncFileDescriptor>  socket;
    void* buffer_;
    std::size_t len_;
};
