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

/**
 * @brief Epoll handler and notification
 * this methods blocks until a the os rises a notification and forwards it
 * to the suspended blocksyscall.
 */
void IOContext::run()
{
    struct epoll_event ev, events[max_events];
    for (;;)
    {
        RS_DBG0("esperando en el epoll");
        auto nfds = epoll_wait(fd_, events, max_events, -1);
        if (nfds == -1)
        {

            RS_FATAL("error", strerror(errno), fd_);
        }

        for (int n = 0; n < nfds; ++n)
        {
            auto socket = static_cast<AsyncFileDescriptor *>(events[n].data.ptr);
            if (!managed_fd.contains(socket))
            {
                RS_INFO("the fd is no longer suscribed, ", (intptr_t)socket, " flags:", (uint32_t)events[n].events, " fd ", (int)events[n].data.fd);
            }
            else
            {
                if (events[n].events & EPOLLIN)
                {

                    RS_DBG0("llamando en in al ptr ", (intptr_t)events[n].data.ptr, " fd ", socket->mFD);
                    if (events[n].events & EPOLLERR)
                    {
                        RS_DBG0("llamando por EPOLLERR");
                    }
                    if (events[n].events & EPOLLRDHUP)
                    {
                        RS_DBG0("llamando por EPOLLRDHUP");
                    }
                    if (events[n].events & EPOLLHUP)
                    {
                        RS_DBG0("llamando por EPOLLHUP");
                        socket->doneRecv_ = true;
                        /**
                         * 
              man epoll: EPOLLHUP
              Hang up happened on the associated file descriptor.

              epoll_wait(2) will always wait for this event; it is not
              necessary to set it in events when calling epoll_ctl().

              Note that when reading from a channel such as a pipe or a
              stream socket, this event merely indicates that the peer
              closed its end of the channel.  Subsequent reads from the
              channel will return 0 (end of file) only after all
              outstanding data in the channel has been consumed.

              jj: the last is not always true... subsequent reads only responds with -1
                        */
                    }
                    if (events[n].events & EPOLLPRI)
                    {
                        RS_DBG0("llamando por EPOLLPRI");
                    }
                    socket->resumeRecv();
                }
                if (events[n].events & EPOLLOUT)
                {
                    RS_DBG0("llamando en out al ptr ", (intptr_t)events[n].data.ptr, " fd ", socket->mFD);
                    socket->resumeSend();
                }
            }
        }
        for (auto *socket : processedSockets)
        {
            auto io_state = socket->io_new_state_;
            if (socket->io_state_ == io_state)
                continue;
            ev.events = io_state;
            ev.data.ptr = socket;
            if (epoll_ctl(fd_, EPOLL_CTL_MOD, socket->mFD, &ev) == -1)
            {
                RS_FATAL("error", strerror(errno), socket->mFD);
                perror("processedSockets new state failed");
                // throw std::runtime_error{"epoll_ctl: mod "+ errno};
                // todo: eliminate
            }
            RS_DBG0("successfully EPOLL_CTL_MOD # ", socket->mFD, " mod ", io_state);
            socket->io_state_ = io_state;
        }
    }
}

/**
 * @brief Attaches a file descriptor to an available for "read" operations.
 *
 * @param socket
 */
void IOContext::attach(AsyncFileDescriptor *socket)
{
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->mFD, &ev) == -1)
    {
        RS_FATAL("error", strerror(errno), socket->mFD);
        perror("epoll_ctl EPOLL_CTL_ADD");
    }
    socket->io_state_ = io_state;

	managed_fd.insert(socket);
	RS_DBG3( "successfully attached FD: ", socket->mFD,
	         " pointer: ", static_cast<intptr_t>(ev.data.ptr) );
}

/**
 * @brief Attaches a file descriptor to an available for "read" operations.
 *
 * @param socket
 */
void IOContext::attachReadonly(AsyncFileDescriptor *socket)
{
    RS_DBG0("ataching RO ...", socket->mFD);
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->mFD, &ev) == -1)
        throw std::runtime_error{"epoll_ctl: attach"};
    socket->io_state_ = io_state;
    RS_DBG0("successfully attached for reading # ", socket->mFD, " pointer ", (intptr_t)ev.data.ptr);
    managed_fd.insert(socket);
}

/**
 * @brief Attaches a file descriptor to an available for "write" operations.
 *
 * @param socket
 */
void IOContext::attachWriteOnly(AsyncFileDescriptor *socket)
{
    RS_DBG0("ataching WO ...", socket->mFD);
    struct epoll_event ev;
    auto io_state = EPOLLOUT | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, socket->mFD, &ev) == -1)
    {
        RS_FATAL("error attaching # ", socket->mFD);
        perror("attachWriteOnly failed");
        // throw std::runtime_error{"epoll_ctl: attach"};
    }
    socket->io_state_ = io_state;
    RS_DBG0("successfully attached for writing events# ", socket->mFD, " pointer ", (uint64_t)ev.data.ptr);
    managed_fd.insert(socket);
}

/**
 * @brief used to update the type of epool subscription to enable reading
 *
 * @param socket
 */
void IOContext::watchRead(AsyncFileDescriptor *socket)
{
    socket->io_new_state_ = socket->io_state_ | EPOLLIN;
    processedSockets.insert(socket);
}

/**
 * @brief used to stop epool notifications for reading events
 *
 * @param socket
 */
void IOContext::unwatchRead(AsyncFileDescriptor *socket)
{
    socket->io_new_state_ = socket->io_state_ & ~EPOLLIN;
    processedSockets.insert(socket);
}

/**
 * @brief used to update the type of epool subscription to enable writing
 *
 * @param socket
 */
void IOContext::watchWrite(AsyncFileDescriptor *socket)
{
    socket->io_new_state_ = socket->io_state_ | EPOLLOUT;
    processedSockets.insert(socket);
}

/**
 * @brief used to stop epool notifications for writing events
 *
 * @param socket
 */
void IOContext::unwatchWrite(AsyncFileDescriptor *socket)
{
    socket->io_new_state_ = socket->io_state_ & ~EPOLLOUT;
    processedSockets.insert(socket);
}

/**
 * @brief Remove an async file descriptor from the notification list
 *
 * @param socket
 */
void IOContext::detach(AsyncFileDescriptor *socket)
{
    RS_DBG0("detaching ", socket->mFD);
    if (epoll_ctl(fd_, EPOLL_CTL_DEL, socket->mFD, nullptr) == -1)
    {
        RS_ERR("epoll_ctl: detach errorrrrrrrrr maybe the fd is not attached");
        // exit(EXIT_FAILURE);
    }
    processedSockets.erase(socket);
    managed_fd.erase(socket);
}
