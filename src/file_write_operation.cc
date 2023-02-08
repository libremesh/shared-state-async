#include "file_write_operation.hh"
#include <iostream>
#include <unistd.h>
#include "async_command.hh"

FileWriteOperation::FileWriteOperation(std::shared_ptr<AsyncFileDescriptor> socket,
        void* buffer,
        std::size_t len)
    : BlockSyscall{}
    , socket{socket}
    , buffer_{buffer}
    , len_{len}
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    socket->io_context_.watchWrite(socket.get());
    std::cout << "FileWriteOperation created\n";
}

FileWriteOperation::~FileWriteOperation()
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    socket->io_context_.unwatchWrite(socket.get());
    std::cout << "~FileWriteOperation\n";
}

ssize_t FileWriteOperation::syscall()
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    std::cout << "FileWriteOperation write(" << socket->fd_ << "," << (char *)buffer_<< "," << len_<< ")\n";
    ssize_t bytes_writen=write(socket->fd_, (char *)buffer_, len_);
    if (bytes_writen == -1)
    {
         std::cout<< "**** error ****" << strerror(errno) << std::endl;
    }
    std::cout<< "bytes read" << bytes_writen << std::endl;
    return bytes_writen;
}

void FileWriteOperation::suspend()
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    socket->coroSend_ = awaitingCoroutine_;
}
