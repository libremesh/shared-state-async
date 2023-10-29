/*
 * Shared State
 *
 * Copyright (C) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include <sys/epoll.h>

#include "io_context.hh"
#include "async_file_desc.hh"
#include "epoll_events_to_string.hh"

#include <util/rsdebuglevel2.h>


/**
 * @brief Epoll handler and notification
 * this methods blocks until a the os rises a notification and forwards it
 * to the suspended blocksyscall.
 */
std::unique_ptr<IOContext> IOContext::setup(std::error_condition* errc)
{
	int epollFD = epoll_create1(0);
	if(epollFD < 0)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errc,
		            "epoll_create1 failed return: ", epollFD );
		return nullptr;
	}

	return std::unique_ptr<IOContext>(new IOContext(epollFD));
}

void IOContext::run()
{
	struct epoll_event ev, events[DEFAULT_MAX_EVENTS];
	for (;;)
	{
		RS_DBG3("Waiting epoll events");
		auto nfds = epoll_wait(mEpollFD, events, DEFAULT_MAX_EVENTS, -1);
		if (nfds == -1)
		{
			std::error_condition* nulli = nullptr;
			rs_error_bubble_or_exit(
			            rs_errno_to_condition(errno), nulli,
			            "epoll_wait failed FD: ", mEpollFD );
		}

		for(int n = 0; n < nfds; ++n)
		{
			uint32_t evFlags = events[n].events;
			int eFD = events[n].data.fd;
			auto aFD = static_cast<AsyncFileDescriptor*>(events[n].data.ptr);

			RS_DBG2( "Got epoll events: ", epoll_events_to_string(evFlags),
			         " eFD: ", eFD, " mFD: ", aFD->mFD,
			         " for aFD: ", reinterpret_cast<intptr_t>(aFD) );

			if (!managed_fd.contains(aFD))
			{
				/* aFD is a dangling pointer at this point so aFD->mFD
				 * it's random stuff, luky you that epoll_data come with the FD
				 * to which the event belong, so we can at least use it for
				 * debugging */
				RS_WARN( "Got stray epoll events: ",
				         epoll_events_to_string(evFlags),
				         " for FD: ", eFD,
				         " aFD: ", reinterpret_cast<intptr_t>(aFD),
				         " which is not subscribed (anymore?)" );
				continue;
			}

			if (evFlags & EPOLLHUP)
			{
				/* man epoll: EPOLLHUP
				 * Hang up happened on the associated file descriptor.
				 * epoll_wait(2) will always wait for this event; it is
				 * not necessary to set it in events when calling
				 * epoll_ctl().
				 *
				 * Note that when reading from a channel such as a pipe
				 * or a stream socket, this event merely indicates that
				 * the peer closed its end of the channel.
				 *
				 * Subsequent reads from the channel will return 0 (end
				 * of file) only after all outstanding data in the
				 * channel has been consumed.
				 * jj: the last is not always true...
				 * subsequent reads only responds with -1
				 */
				aFD->doneRecv_ = true;
			}

			aFD->resumePendingOps();

#if 0
			if (evFlags & EPOLLIN)
			{
				aFD->resumePendingOps();
			}
			if(evFlags & EPOLLOUT)
			{
				bool socketHasOut = aFD->getNewIoState() & EPOLLOUT;
				RS_DBG1("FD: ", aFD->mFD, " Got EPOLLOUT",
				        " which has EPOLLOUT? ", socketHasOut? "yes" : "no" );

				aFD->resumePendingOps();
			}
#endif
		}

		for (auto* socket : processedSockets)
		{
			/* New state does actually just have EPOLLIN or EPOLLOUT
			 * EPOLLET is always needed to work with coroutines so set it always
			 * here, maybe there is more elegant solution but I haven't thinked
			 * about it yet */
			auto io_state = socket->getNewIoState() | EPOLLET;
			if (socket->getIoState() == io_state) continue;

			ev.events = io_state;
			ev.data.ptr = socket;
			if (epoll_ctl(mEpollFD, EPOLL_CTL_MOD, socket->mFD, &ev) == -1)
			{
				/* ATM this has happened only with standard input FD 0, with
				 * errno 2 No such file or directory */
				RS_ERR( "Failed to update epoll IO state for "
				         "FD: ", socket->mFD, " ",
				        rs_errno_to_condition(errno) );
			}
			socket->setIoState(io_state);
		}
	}
}

IOContext::IOContext(int epollFD): mEpollFD(epollFD) {}

/**
 * @brief Attaches a file descriptor to an available for "read" operations.
 *
 * @param socket
 */
void IOContext::attach(AsyncFileDescriptor *socket)
{
	/* TODO: This seems to be the exact same og IOContext::attachReadonly
	 * check which one makes sense to keep of the two */

	RS_DBG4("fd: ", socket->mFD);

    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
	if (epoll_ctl(mEpollFD, EPOLL_CTL_ADD, socket->mFD, &ev) == -1)
	{
		RS_ERR(rs_errno_to_condition(errno), " mFD: ", socket->mFD);
        perror("epoll_ctl EPOLL_CTL_ADD");
    }
	socket->setIoState(io_state);

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
	RS_DBG4("fd: ", socket->mFD);
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
	if (epoll_ctl(mEpollFD, EPOLL_CTL_ADD, socket->mFD, &ev) == -1)
        throw std::runtime_error{"epoll_ctl: attach"};
	socket->setIoState(io_state);
	RS_DBG4( "successfully attached for reading mFD: ", socket->mFD,
	         " pointer: ", (intptr_t)ev.data.ptr );
    managed_fd.insert(socket);
}

/**
 * @brief Attaches a file descriptor to an available for "write" operations.
 *
 * @param socket
 */
void IOContext::attachWriteOnly(AsyncFileDescriptor *socket)
{
	RS_DBG4("mFD: ", socket->mFD);

    struct epoll_event ev;
    auto io_state = EPOLLOUT | EPOLLET;
    ev.events = io_state;
    ev.data.ptr = socket;
	if (epoll_ctl(mEpollFD, EPOLL_CTL_ADD, socket->mFD, &ev) == -1)
	{
		RS_ERR( "failure attaching mFD ", socket->mFD, " ",
		        rs_errno_to_condition(errno) );
	}
	socket->setIoState(io_state);
	managed_fd.insert(socket);

	RS_DBG4( "fd: ", socket->mFD, " ptr: ", (uint64_t)ev.data.ptr,
	         " getNewIoState() " , socket->getNewIoState() );
}

/**
 * @brief used to update the type of epool subscription to enable reading
 *
 * @param socket
 */
void IOContext::watchRead(AsyncFileDescriptor *socket)
{
	socket->setNewIoState(socket->getNewIoState() | EPOLLIN);
	processedSockets.insert(socket);

	RS_DBG4("mFD: ", socket->mFD,
	        " getNewIoState() " , socket->getNewIoState() );
}

/**
 * @brief used to stop epool notifications for reading events
 *
 * @param socket
 */
void IOContext::unwatchRead(AsyncFileDescriptor *socket)
{
	socket->setNewIoState(socket->getNewIoState() & ~EPOLLIN);
	processedSockets.insert(socket);

	RS_DBG4("mFD: ", socket->mFD,
	        " getNewIoState() " , socket->getNewIoState() );
}

/**
 * @brief used to update the type of epool subscription to enable writing
 *
 * @param socket
 */
void IOContext::watchWrite(AsyncFileDescriptor *socket)
{
	socket->setNewIoState(socket->getNewIoState() | EPOLLOUT);
	processedSockets.insert(socket);

	RS_DBG4("mFD: ", socket->mFD,
	        " getNewIoState() " , socket->getNewIoState() );
}

/**
 * @brief used to stop epool notifications for writing events
 *
 * @param socket
 */
void IOContext::unwatchWrite(AsyncFileDescriptor *socket)
{
	socket->setNewIoState(socket->getNewIoState() & ~EPOLLOUT);
	processedSockets.insert(socket);

	RS_DBG4("mFD: ", socket->mFD,
	        " getNewIoState() " , socket->getNewIoState() );
}

/**
 * @brief Remove an async file descriptor from the notification list
 *
 * @param socket
 */
void IOContext::detach(AsyncFileDescriptor* aFD, std::error_condition* errbub)
{
	RS_DBG4("FD: ", aFD->mFD);

	if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, aFD->mFD, nullptr) == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            " failed EPOLL_CTL_DEL for FD: ", aFD->mFD );
		RS_ERR("epoll_ctl failure detaching: ", rs_errno_to_condition(errno));
	}

	discard(*aFD);
}

void IOContext::discard(AsyncFileDescriptor& aFD)
{
	RS_DBG4("FD: ", aFD->mFD);

	processedSockets.erase(&aFD);
	managed_fd.erase(&aFD);
}
