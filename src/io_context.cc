#include "io_context.hh"
#include <stdexcept>
#include "socket.hh"

//Epoll handler and notification
void IOContext::run()
{
    struct epoll_event ev, events[max_events];
    for (;;)
    {
        auto nfds = epoll_wait(fd_, events, max_events, -1);
        if (nfds == -1)
            throw std::runtime_error{"epoll_wait"};

        for (int n = 0; n < nfds; ++n)
        {
            auto socket = static_cast<Socket*>(events[n].data.ptr);

            if (events[n].events & EPOLLIN)
                socket->resumeRecv();
            if (events[n].events & EPOLLOUT)
                socket->resumeSend();
        }
        for (auto* socket : processedSockets)
        {
            auto io_state = socket->io_new_state_;
            if (socket->io_state_ == io_state)
                continue;
            ev.events = io_state;
            ev.data.ptr = socket;
            if (epoll_ctl(fd_, EPOLL_CTL_MOD, socket->fd_, &ev) == -1)
                throw std::runtime_error{"epoll_ctl: mod"};
                //todo: eliminate
            socket->io_state_ = io_state;
        }
    }
}

void IOContext::attach(Socket* socket)
{
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->fd_, &ev) == -1)
        throw std::runtime_error{"epoll_ctl: attach"};
    socket->io_state_ = io_state;
}

void IOContext::attachreadonly(Socket* socket)
{
    struct epoll_event ev;
    auto io_state = EPOLLIN;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->fd_, &ev) == -1)
        throw std::runtime_error{"epoll_ctl: attach"};
    socket->io_state_ = io_state;
    std::cout << "successfully attached # " <<  socket->fd_ << std::endl;;
}

void IOContext::watchRead(Socket* socket)
{
    socket->io_new_state_ = socket->io_state_ | EPOLLIN;
    processedSockets.insert(socket);
}

void IOContext::unwatchRead(Socket* socket)
{
    socket->io_new_state_ = socket->io_state_ & ~EPOLLIN;
    processedSockets.insert(socket);
}

void IOContext::watchWrite(Socket* socket)
{
    socket->io_new_state_ = socket->io_state_ | EPOLLOUT;
    processedSockets.insert(socket);
}

void IOContext::unwatchWrite(Socket* socket)
{
    socket->io_new_state_ = socket->io_state_ & ~EPOLLOUT;
    processedSockets.insert(socket);
}

void IOContext::detach(Socket* socket)
{
    struct epoll_event ev;
    auto io_state = EPOLLIN;
    ev.events = io_state;
    std::cout<< "detaching ##" << socket->fd_ << std::endl;

    if (epoll_ctl(fd_, EPOLL_CTL_DEL, socket->fd_, &ev) == -1) {
        perror("epoll_ctl: detach");
        exit(EXIT_FAILURE);
    }
    socket->fd_=-1;
    processedSockets.erase(socket);
}
