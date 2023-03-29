/*
 * Shared State
 *
 * Copyright (c) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (c) 2023  Instituto Nacional de Tecnología Industrial
 * Copyright (C) 2023  Asociación Civil Altermundi <info@altermundi.net>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include "io_context.hh"
#include <stdexcept>
#include "async_file_desc.hh"

// Epoll handler and notification
void IOContext::run()
{
    struct epoll_event ev, events[max_events];
    for (;;)
    {
        RS_DBG0("esperando en el epoll");
        auto nfds = epoll_wait(fd_, events, max_events, -1);
        if (nfds == -1)
        {

            RS_FATAL("error" ,strerror(errno) , fd_);
            perror("processedSockets new state failed");
        }

        for (int n = 0; n < nfds; ++n)
        {
            auto socket = static_cast<AsyncFileDescriptor *>(events[n].data.ptr);

            if (events[n].events & EPOLLIN)
            {
                RS_DBG0("llamando en in");
                socket->resumeRecv();
            }
            if (events[n].events & EPOLLOUT)
            {
                RS_DBG0("llamando en out");

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
                RS_FATAL("error", strerror(errno), socket->fd_);
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
    RS_DBG0("ataching");
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->fd_, &ev) == -1)
    {
        RS_FATAL("error", strerror(errno), socket->fd_);
        perror("epoll_ctl EPOLL_CTL_ADD");
    }
    socket->io_state_ = io_state;
}

void IOContext::attachReadonly(AsyncFileDescriptor *socket)
{
    RS_DBG0("ataching RO ..." , socket->fd_ );
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->fd_, &ev) == -1)
        throw std::runtime_error{"epoll_ctl: attach"};
    socket->io_state_ = io_state;
    RS_DBG0("successfully attached for reading # " ,socket->fd_ );
    ;
}

void IOContext::attachWriteOnly(AsyncFileDescriptor *socket)
{
    RS_DBG0("ataching WO ..." , socket->fd_ );
    struct epoll_event ev;
    auto io_state = EPOLLOUT | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->fd_, &ev) == -1)
    {
        RS_FATAL("error attaching # " , socket->fd_ );
        perror("attachWriteOnly failed");
        // throw std::runtime_error{"epoll_ctl: attach"};
    }
    socket->io_state_ = io_state;
    RS_DBG0("successfully attached for writing events# " , socket->fd_ );
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
        // TODO: fix
        RS_FATAL("epoll_ctl: detach");
        exit(EXIT_FAILURE);
    }
    processedSockets.erase(socket);
}
