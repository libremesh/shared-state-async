#include "socket.hh"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

Socket::Socket(std::string_view port, IOContext& io_context)
    : AsyncFileDescriptor(io_context) 
    //: io_context_{io_context} //non static data_member initialization
{
    struct addrinfo hints, *res;

    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  //Use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; //TCP ... if you require UDP you must use SOCK_DGRAM
    hints.ai_flags = AI_PASSIVE;     //Fill in my IP for me

    getaddrinfo(NULL, port.data(), &hints, &res);
    fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    //todo: error ?
    int opt;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (bind(fd_, res->ai_addr, res->ai_addrlen) == -1)
        throw std::runtime_error{"bind"};
        //todo: fix
    listen(fd_, 8);
    //todo:error check
    fcntl(fd_, F_SETFL, O_NONBLOCK);
    io_context_.attach(this);
    io_context_.watchRead(this);
}

Socket::~Socket()
{
    std::cout << "delete the socket(" << fd_ << ")\n";
    if (fd_ == -1)
        return;
    io_context_.detach(this);
    close(fd_);
}

std::task<std::shared_ptr<Socket>> Socket::accept()
{
    //todo: deberia devolver unique 
    int fd = co_await SocketAcceptOperation{this};
    if (fd == -1)
        throw std::runtime_error{"accept"};
        //todo:
    std::cout << "aceptando";
    co_return std::shared_ptr<Socket>(new Socket{fd, io_context_});
}

SocketRecvOperation Socket::recv(void* buffer, std::size_t len)
{
    return SocketRecvOperation{this, buffer, len};
}
SocketSendOperation Socket::send(void* buffer, std::size_t len)
{
    return SocketSendOperation{this, buffer, len};
}

Socket::Socket(int fd, IOContext& io_context):AsyncFileDescriptor(fd,io_context)
{}
