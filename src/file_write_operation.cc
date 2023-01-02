#include "file_write_operation.hh"
#include <iostream>
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
    std::cout << "socket_fileRead_operation created\n";
}

FileWriteOperation::~FileWriteOperation()
{
    socket->io_context_.unwatchWrite(socket);
    std::cout << "~socket_recv_operation\n";
}

ssize_t FileWriteOperation::syscall()
{
    std::cout << "write(" << socket->fd_ << ", buffer_, len_, 0)\n";
    ssize_t bytes_writen=write(socket->fd_, (char *)buffer_, len_);
    close(socket->fd_);
    return bytes_writen;
}

void FileWriteOperation::suspend()
{
    socket->coroRecv_ = awaitingCoroutine_;
}
