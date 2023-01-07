#include "file_read_operation.hh"
#include <iostream>
#include "async_command.hh"
#include "async_file_desc.hh"

FileReadOperation::FileReadOperation(AsyncFileDescriptor* socket,
        void* buffer,
        std::size_t len)
    : BlockSyscall{}
    , socket{socket}
    , buffer_{buffer}
    , len_{len}
{
    socket->io_context_.watchRead(socket);
    std::cout << "fileRead_operation created\n";
}

FileReadOperation::~FileReadOperation()
{
    socket->io_context_.unwatchRead(socket);
    std::cout << "~fileRead_operation\n";
}

ssize_t FileReadOperation::syscall()
{
    std::cout << "reading(" << socket->fd_ << ", buffer_, len_, 0)\n";
    ssize_t bytesread = read(socket->fd_, (char *)buffer_, len_);
    while (bytesread == -1)
    {
        std::cout<< "**** error ****" << strerror(errno) << std::endl;
        sleep(1);
        bytesread = read(socket->fd_, (char *)buffer_, len_);

    }
    
    std::cout<<"Read bytes" << bytesread << "content" << (char *)buffer_;
    return bytesread;
}

void FileReadOperation::suspend()
{
    socket->coroRecv_ = awaitingCoroutine_;
}
