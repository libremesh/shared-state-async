#include "file_write_operation.hh"
#include <iostream>
#include <unistd.h>
#include "async_command.hh"

FileWriteOperation::FileWriteOperation(AsyncFileDescriptor* socket,
        void* buffer,
        std::size_t len)
    : BlockSyscall{}
    , socket{socket}
    , buffer_{buffer}
    , len_{len}
{
    socket->io_context_.watchWrite(socket);
    std::cout << "FileWriteOperation created\n";
}

FileWriteOperation::~FileWriteOperation()
{
    socket->io_context_.unwatchWrite(socket);
    std::cout << "~FileWriteOperation\n";
}

ssize_t FileWriteOperation::syscall()
{
    std::cout << "write(" << socket->fd_ << "," << (char *)buffer_<< "," << len_<< ")\n";
    ssize_t bytes_writen=write(socket->fd_, (char *)buffer_, len_);
    return bytes_writen;
}

void FileWriteOperation::suspend()
{
    socket->coroRecv_ = awaitingCoroutine_;
}
