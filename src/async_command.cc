#include "socket.hh"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "async_command.hh"
#include <iostream>

AsyncCommand::AsyncCommand(FILE * fdFromStream, AsyncFileDescriptor* socket)
    :AsyncFileDescriptor(socket->io_context_), pipe{fdFromStream}
{
    fd_=fileno(fdFromStream);

    int flags = fcntl(fd_, F_GETFL, 0);
    //put into "nonblocking mode"
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    io_context_.attachreadonly(this);
    //io_context_.watchRead(this);
    std::cout<< "AsyncCommand created and filedescriptor # " << fd_ << std::endl;
}



AsyncCommand::~AsyncCommand()
{
    std::cout << "delete the socket(" << fd_ << ")\n";
    if (fd_ == -1)
        return;
    io_context_.detach(this);
    close(fd_);
}

// std::task<std::shared_ptr<AsyncCommand>> AsyncCommand::accept()
// {
//     //todo: deberia devolver unique 
//     int fd = co_await AsyncCommandAcceptOperation{this};
//     if (fd == -1)
//         throw std::runtime_error{"accept"};
//         //todo:
//     std::cout << "aceptando";
//     co_return std::shared_ptr<AsyncCommand>(new AsyncCommand{fd, io_context_});
// }



FileReadOperation AsyncCommand::recvfile(void* buffer, std::size_t len)
{
    return FileReadOperation{this, buffer, len};
}

//AsyncCommand::AsyncCommand(int fd, IOContext& io_context):AsyncFileDescriptor(fd,io_context)
//{}

// AsyncCommand::AsyncCommand(int fd, IOContext& io_context)
//     : io_context_{io_context}
//     , fd_{fd}
// {
//     fcntl(fd_, F_SETFL, O_NONBLOCK);
//     io_context_.attach(this);
// }