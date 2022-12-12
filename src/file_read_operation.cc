#include "file_read_operation.hh"
#include <iostream>
#include "async_command.hh"

FileReadOperation::FileReadOperation(AsyncCommand* socket,
        void* buffer,
        std::size_t len)
    : BlockSyscall{}
    , socket{socket}
    , buffer_{buffer}
    , len_{len}
{
    socket->io_context_.watchRead(socket);
    std::cout << "socket_fileRead_operation created\n";
}

FileReadOperation::~FileReadOperation()
{
    socket->io_context_.unwatchRead(socket);
    std::cout << "~socket_recv_operation\n";
}

ssize_t FileReadOperation::syscall()
{
    std::string result;
    std::cout << "fgets(" << fileno(socket->pipe) << ", buffer_, len_, 0)\n";
    while (!feof(socket->pipe))
    {
        if (fgets((char *)buffer_, len_, socket->pipe) != nullptr)
            result += (char *)buffer_;
    }
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());

    return 10;
    //todo: fix this
}

void FileReadOperation::suspend()
{
    socket->coroRecv_ = awaitingCoroutine_;
}
