#include "io_context.hh"
#include <stdexcept>
#include "async_file_desc.hh"

// Epoll handler and notification
void IOContext::run()
{
    struct epoll_event ev, events[max_events];
    for (;;)
    {
        std::cout << "esperando en el epoll" << std::endl;
        auto nfds = epoll_wait(fd_, events, max_events, -1);
        if (nfds == -1)
            {
                
            throw std::runtime_error{"epoll_wait"};
            }

        for (int n = 0; n < nfds; ++n)
        {
            auto socket = static_cast<AsyncFileDescriptor *>(events[n].data.ptr);

            if (events[n].events & EPOLLIN)
            {
                std::cout << "llamando en in" << std::endl;
                socket->resumeRecv();
            }
            if (events[n].events & EPOLLOUT)
            {
                std::cout << "llamando en out" << std::endl;

                socket->resumeSend();
            }
        }
        for (auto *socket : processedSockets)
        {
            auto io_state = socket->io_new_state_;
            if (socket->io_state_ == io_state)
                continue;
            ev.events = io_state;
            ev.data.ptr = socket;
            if (epoll_ctl(fd_, EPOLL_CTL_MOD, socket->fd_, &ev) == -1)
            {
                std::cout << "error" << strerror(errno) << socket->fd_;
                perror("processedSockets new state failed");
                // throw std::runtime_error{"epoll_ctl: mod "+ errno};
                // todo: eliminate
            }
            socket->io_state_ = io_state;
        }
    }
}

void IOContext::attach(AsyncFileDescriptor *socket)
{
    std::cout << "ataching ..." << std::endl;
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->fd_, &ev) == -1)
        throw std::runtime_error{"epoll_ctl: attach"};
    socket->io_state_ = io_state;
}

void IOContext::attachReadonly(AsyncFileDescriptor *socket)
{
    std::cout << "ataching RO ..." << socket->fd_ << std::endl;
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->fd_, &ev) == -1)
        throw std::runtime_error{"epoll_ctl: attach"};
    socket->io_state_ = io_state;
    std::cout << "successfully attached for reading # " << socket->fd_ << std::endl;
    ;
}

void IOContext::attachWriteOnly(AsyncFileDescriptor *socket)
{
    std::cout << "ataching WO ..." << socket->fd_ << std::endl;
    struct epoll_event ev;
    auto io_state = EPOLLOUT | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
    std::cout << fcntl(socket->fd_, F_GETFL) << " flags" << std::endl;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->fd_, &ev) == -1)
    {
        std::cout << "error attaching # " << socket->fd_ << std::endl;
        std::cout << "error" << strerror(errno);
        perror("attachWriteOnly failed");
        // throw std::runtime_error{"epoll_ctl: attach"};
    }
    socket->io_state_ = io_state;
    std::cout << "successfully attached for writing events# " << socket->fd_ << std::endl;
    ;
}

void IOContext::watchRead(AsyncFileDescriptor *socket)
{
    socket->io_new_state_ = socket->io_state_ | EPOLLIN;
    processedSockets.insert(socket);
}

void IOContext::unwatchRead(AsyncFileDescriptor *socket)
{
    socket->io_new_state_ = socket->io_state_ & ~EPOLLIN;
    processedSockets.insert(socket);
}

void IOContext::watchWrite(AsyncFileDescriptor *socket)
{
    socket->io_new_state_ = socket->io_state_ | EPOLLOUT;
    processedSockets.insert(socket);
}

void IOContext::unwatchWrite(AsyncFileDescriptor *socket)
{
    socket->io_new_state_ = socket->io_state_ & ~EPOLLOUT;
    processedSockets.insert(socket);
}

void IOContext::detach(AsyncFileDescriptor *socket)
{
    if (epoll_ctl(fd_, EPOLL_CTL_DEL, socket->fd_, nullptr) == -1)
    {
        perror("epoll_ctl: detach");
        exit(EXIT_FAILURE);
    }
    processedSockets.erase(socket);
}
