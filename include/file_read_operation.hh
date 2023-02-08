#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include "block_syscall.hh"
#include <memory>

class AsyncFileDescriptor;

class FileReadOperation : public BlockSyscall<FileReadOperation, ssize_t>
{
public:
    FileReadOperation(std::shared_ptr<AsyncFileDescriptor> socket, void* buffer, std::size_t len);
    ~FileReadOperation();

    ssize_t syscall();
    void suspend();
private:
    std::shared_ptr<AsyncFileDescriptor> socket;
    void* buffer_;
    std::size_t len_;
};
