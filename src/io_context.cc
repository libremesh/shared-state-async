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
#include <fcntl.h>

#include "io_context.hh"
#include "async_file_descriptor.hh"
#include "epoll_events_to_string.hh"

#include <util/rsdebug.h>
#include <util/stacktrace.h>
#include <util/rsdebuglevel1.h>


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
	epoll_event events[DEFAULT_MAX_EVENTS];
	for (;;)
	{
		RS_DBG3("Waiting epoll events");
		auto nfds = epoll_wait(mEpollFD, events, DEFAULT_MAX_EVENTS, -1);
		if (nfds == -1)
		{
#if RS_DEBUG_LEVEL > 0
			/* While running under GDB epoll_wait easily to incurr into
			 * erro 4 EINTR Interrupted system call, due to debugging,
			 * in that case should it should be re-attempted
			 * @see https://stackoverflow.com/a/2253013 */
			if(errno == EINTR)
			{
				RS_DBG( "epoll_wait got EINTR, probably interrupted due to "
				        "attached debugger, retrying" );
				continue;
			}
#endif //  RS_DEBUG_LEVEL > 0
			rs_error_bubble_or_exit(
			            rs_errno_to_condition(errno), nullptr,
			            "epoll_wait failed FD: ", mEpollFD );
		}

		for(int n = 0; n < nfds; ++n)
		{
			uint32_t evFlags = events[n].events;
			int mFD = events[n].data.fd;

			auto findIt = mManagedFD.find(mFD);
			if (findIt == mManagedFD.end())
			{
				/* While testing we have been getting EPOLLERR regularly on
				 * not-subscribed-anymore FD. For example after finish reading
				 * on a timer FD, so let's avoid flooding the log for this
				 * "normal" (yet not fully understood) circumstance unless
				 * debugging is enabled. */
				if(RS_DEBUG_LEVEL > 2 || evFlags != EPOLLERR)
					RS_WARN( "Got stray epoll events: ",
					         epoll_events_to_string(evFlags),
					         " for FD: ", mFD,
					         " which is not subscribed (anymore)" );

				continue;
			}

			/* NEED a FULL blown shared_ptr copy here, I haven't fully
			 * understood yet why taking just a reference to it in this place
			 * cause invalid memory read, expecially when some error occurs,
			 * detected using valgrind */
			auto aFD = findIt->second;

			RS_DBG2(*aFD, " got epoll events: ", epoll_events_to_string(evFlags));

			aFD->resumePendingOps(evFlags);
		}

		for (auto&& mEl : std::as_const(mManagedFD))
		{
			/* Don't need a full blown shared_ptr costly copy here just take a
			 * reference to it */
			auto& aFD = mEl.second;

			/* New state does actually just have EPOLLIN or EPOLLOUT
			 * EPOLLET is always needed to work with coroutines so set it always
			 * here, maybe there is more elegant solution but I haven't thinked
			 * about it yet */
			auto io_state = aFD->getNextIoState() | EPOLLET;
			if (aFD->getIoState() == io_state) continue;

			epoll_event ev;
			ev.events = io_state;
			ev.data.fd = aFD->getFD();
			if (epoll_ctl(mEpollFD, EPOLL_CTL_MOD, aFD->getFD(), &ev) == -1)
			{
				/* I have seen this happening only with standard input FD 0,
				 * with errno 2 No such file or directory */
				RS_ERR( "Failed to update epoll IO state for ", *aFD,
				        " getIoState(): ",
				        epoll_events_to_string(aFD->getIoState()),
				        " next io_state: ",
				        epoll_events_to_string(aFD->getNextIoState()), " ",
				        rs_errno_to_condition(errno) );
			}
			aFD->setIoState(io_state);
		}
	}
}

/**
 * @brief Attaches a file descriptor to an available for "read" operations.
 *
 * @param socket
 */
void IOContext::attach(AsyncFileDescriptor* aFD)
{
	/* TODO: This seems to be the exact same og IOContext::attachReadonly
	 * check which one makes sense to keep of the two, show we add EPOLLOUT
	 * flag here? */

	RS_DBG4(*socket);

    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
	ev.data.fd = aFD->getFD();
	if (epoll_ctl(mEpollFD, EPOLL_CTL_ADD, aFD->getFD(), &ev) == -1)
	{
		RS_ERR( "EPOLL_CTL_ADD failed for: ", *aFD, " ",
		        rs_errno_to_condition(errno) );
	}
	aFD->setIoState(io_state);

	RS_DBG3("successfully attached ", *socket);
}

/**
 * @brief Attaches a file descriptor to an available for "read" operations.
 *
 * @param socket
 */
void IOContext::attachReadonly(AsyncFileDescriptor* aFD)
{
	RS_DBG4("fd: ", socket->mFD);
    struct epoll_event ev;
    auto io_state = EPOLLIN | EPOLLET;
    ev.events = io_state;
	ev.data.fd = aFD->getFD();
	if (epoll_ctl(mEpollFD, EPOLL_CTL_ADD, aFD->getFD(), &ev) == -1)
	{
		RS_ERR( "EPOLL_CTL_ADD failed for: ", *aFD, " ",
		        rs_errno_to_condition(errno) );
	}

	aFD->setIoState(io_state);
	RS_DBG3("successfully attached ", *socket);
}

/**
 * @brief Attaches a file descriptor to an available for "write" operations.
 *
 * @param socket
 */
void IOContext::attachWriteOnly(AsyncFileDescriptor* aFD)
{
	RS_DBG4("mFD: ", socket->mFD);

    struct epoll_event ev;
    auto io_state = EPOLLOUT | EPOLLET;
    ev.events = io_state;
	ev.data.fd = aFD->getFD();
	if (epoll_ctl(mEpollFD, EPOLL_CTL_ADD, aFD->getFD(), &ev) == -1)
	{
		RS_ERR( "EPOLL_CTL_ADD failed for: ", *aFD, " ",
		        rs_errno_to_condition(errno) );
	}
	aFD->setIoState(io_state);

	RS_DBG3( "successfully attached ", *aFD, " for: ",
	         epoll_events_to_string(io_state) );
}

/**
 * @brief used to update the type of epool subscription to enable reading
 *
 * @param socket
 */
void IOContext::watchRead(AsyncFileDescriptor *socket)
{
	socket->setNextIoState(socket->getNextIoState() | EPOLLIN);

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
	socket->setNextIoState(socket->getNextIoState() & ~EPOLLIN);

	RS_DBG4("mFD: ", *socket,
	        " getNewIoState() " , socket->getNewIoState() );
}

/**
 * @brief used to update the type of epool subscription to enable writing
 *
 * @param socket
 */
void IOContext::watchWrite(AsyncFileDescriptor *socket)
{
	socket->setNextIoState(socket->getNextIoState() | EPOLLOUT);

	RS_DBG4("mFD: ", *socket,
	        " getNewIoState() " , socket->getNewIoState() );
}

/**
 * @brief used to stop epool notifications for writing events
 *
 * @param socket
 */
void IOContext::unwatchWrite(AsyncFileDescriptor *socket)
{
	socket->setNextIoState(socket->getNextIoState() & ~EPOLLOUT);

	RS_DBG4("mFD: ", socket->mFD,
	        " getNewIoState() " , socket->getNewIoState() );
}

std::ostream &operator<<(std::ostream& out, const IOContext& ioContext)
{
	out << " ioContext: " << &ioContext << " epoll FD: " << ioContext.mEpollFD
		<< " managed FDs: " << ioContext.mManagedFD.size() << " [ ";
	for(auto& tKV : std::as_const(ioContext.mManagedFD))
		out << "{" << tKV.first << ", " << tKV.second << "} ";
	out << "]";
	return out;
}
