#include "socket_accept_operation.hh"

#include <iostream>

#include "socket.hh"

SocketAcceptOperation::SocketAcceptOperation(Socket* socket)
    : BlockSyscall{}
    , socket{socket}
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    socket->io_context_.watchRead(socket);
    std::cout << "socket_accept_operation\n";
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;

}

SocketAcceptOperation::~SocketAcceptOperation()
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    socket->io_context_.unwatchRead(socket);
    std::cout << "~socket_accept_operation\n";
    std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

}

int SocketAcceptOperation::syscall()
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof their_addr;
    std::cout << "accept(" << socket->fd_ << ", ...)"<<  std::endl;
    return accept(socket->fd_, (struct sockaddr *) &their_addr, &addr_size);
}

void SocketAcceptOperation::suspend()
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    socket->coroRecv_ = awaitingCoroutine_;
}
