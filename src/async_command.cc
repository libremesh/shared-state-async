#include "socket.hh"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "async_command.hh"
#include <iostream>

AsyncCommand::AsyncCommand(FILE *fdFromStream, AsyncFileDescriptor *socket)
    : AsyncFileDescriptor(socket->io_context_), pipe{fdFromStream}
{
    fd_ = fileno(fdFromStream);

    int flags = fcntl(fd_, F_GETFL, 0);
    // put into "nonblocking mode"
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    io_context_.attachreadonly(this);
    // io_context_.watchRead(this);
    std::cout << "AsyncCommand created and filedescriptor # " << fd_ << std::endl;
}

AsyncCommand::AsyncCommand(std::string cmd, AsyncFileDescriptor *socket): AsyncFileDescriptor(socket->io_context_)
{
    pipe = popen(cmd.c_str(), "r");
    // partir el popen
    if (!pipe)
    {
        std::cout<<"we have a problem...";
    }
        
    fd_ = fileno(pipe);

    int flags = fcntl(fd_, F_GETFL, 0);
    // put into "nonblocking mode"
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    io_context_.attachreadonly(this);
    // io_context_.watchRead(this);
    std::cout << "AsyncCommand created and filedescriptor # " << fd_ << std::endl;
}



AsyncCommand::~AsyncCommand()
{
    std::cout << "delete the AsyncCommand(" << fd_ << ")\n";
    if (fd_ == -1)
        return;
    io_context_.detach(this);
    close(fd_);
    pclose(pipe);
    
}

FileReadOperation AsyncCommand::recvfile(void *buffer, std::size_t len)
{
    return FileReadOperation{this, buffer, len};
}
