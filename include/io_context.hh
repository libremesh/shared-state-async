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

#pragma once

#include <map>
#include <memory>
#include <type_traits>
#include <fcntl.h>
#include <sys/epoll.h>

#include "task.hh"
#include "async_file_descriptor.hh"
#include "close_operation.hh"

#include <util/rsdebug.h>
#include <util/stacktrace.h>
#include <util/rsdebuglevel2.h>


/**
 * This class works as dispacher, notifying suspended blocksyscall when
 * ready to be activated. Blocksyscalls must suscribe to this class in order
 * to be notified.
 *
 * @brief This class is an Just an epoll wrapper 
 */
class IOContext
{
public:
	static std::unique_ptr<IOContext> setup(
	        std::error_condition* errc = nullptr );

	void run();

	template<class AFD_T, typename /* = AsyncFileDescriptor and derivatives */>
	std::shared_ptr<AFD_T> registerFD(
	        int fd, std::error_condition* errbub = nullptr );

	/**
	 * @tparam AFD_T AsyncFileDescriptor or derivatives
	 */
	template<class AFD_T>
	std::task<bool> closeAFD(
	        std::shared_ptr<AFD_T> aFD, std::error_condition* errbub = nullptr );

	// TODO: Take a reference to an AsyncFileDescriptor instead of a pointer
	// TODO: This methods can fail add error bubbling paramether
    void attach(AsyncFileDescriptor* socket);
    void attachReadonly(AsyncFileDescriptor* socket);
    void attachWriteOnly(AsyncFileDescriptor* socket);
    void watchRead(AsyncFileDescriptor* socket);
    void unwatchRead(AsyncFileDescriptor* socket);
    void watchWrite(AsyncFileDescriptor* socket);
    void unwatchWrite(AsyncFileDescriptor* socket);

	/// Debugging helper
	friend std::ostream &operator<<(std::ostream& out, const IOContext& ioContext);

private:
	static constexpr int DEFAULT_MAX_EVENTS = 20;

	IOContext(int epollFD): mEpollFD(epollFD) {}

	const int mEpollFD;

	/** Map OS file descriptor to managed AsyncFileDescriptor */
	std::map<int, std::shared_ptr<AsyncFileDescriptor>> mManagedFD;
};

template<class AFD_T = AsyncFileDescriptor,
         typename = std::enable_if_t<std::is_base_of_v<AsyncFileDescriptor, AFD_T>>>
std::shared_ptr<AFD_T> IOContext::registerFD(
        int fd, std::error_condition* errbub )
{
	auto flags = fcntl(fd, F_GETFL, 0);
	if(flags == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            " failure getting FD: ", fd, " flags" );
		return nullptr;
	}

	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            " failure setting FD: ", fd, " non-blocking" );
		return nullptr;
	}

	auto aFD = std::shared_ptr<AFD_T>(
	            new AFD_T(fd, *this) );
	mManagedFD[fd] = aFD;

	return aFD;
}

class AsyncCommand;

template<class AFD_T>
std::task<bool> IOContext::closeAFD(
        std::shared_ptr<AFD_T> aFD,
        std::error_condition* errbub )
{
	static_assert( std::is_base_of<AsyncFileDescriptor, AFD_T>::value,
				  "AsyncFileDescriptor or derivative required" );
	static_assert( !std::is_same<AsyncCommand, AFD_T>::value,
				   "PipedAsyncCommand have its own specialization" );

	/* OBXIOUS PEDANTINC CHECK
	 * Considering the way IOContext and AsyncFileDescriptor code are structured
	 * this should really never happen unless you, seriously attempt to shoot
	 * your own foot messing with memory, ponters etc. on purpose, but being
	 * close() so "peculiar" let's check for this too.
	 * If one of the situations marked with this comment happens it is an
	 * obvious symptom of a pathologically unrecoverable state so terminate the
	 * program ASAP without further error bubbling.
	 */

	if(!aFD)
	{
		// @see OBXIOUS PEDANTINC CHECK
		rs_error_bubble_or_exit(
			std::errc::invalid_argument, errbub,
			"Attempt closing nullpointer AFD ", *this );
		co_return false;
	}

	auto fdIt = mManagedFD.find(aFD->mFD);
	if(fdIt == mManagedFD.end()) RS_UNLIKELY
	{
		// @see OBXIOUS PEDANTINC CHECK
		rs_error_bubble_or_exit(
			std::errc::no_such_file_or_directory, errbub,
			"Attempt closing AFD ", *aFD, " not managed by IOContext ", this,
			" ", *this );
		co_return false;
	}

	if(fdIt->second != aFD) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
			std::errc::state_not_recoverable, nullptr, // Force exit
			"mismatching AFD and FD ", aFD, " ", *aFD, " IOContext: ", this,
			" ", *this );
		co_return false;
	}

	/* To avoid stray events on closed socket that still live on kernel side
	 * unsubscribe them from epoll instead of relying on epoll silently removing
	 * them when finally closed kernel side
	 * @see https://stackoverflow.com/a/46987706 */
	if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, aFD->mFD, nullptr) == -1) RS_UNLIKELY
	{
		// @see OBXIOUS PEDANTINC CHECK
		rs_error_bubble_or_exit(
			rs_errno_to_condition(errno), nullptr, // Force exit
			"failure removing FD from epoll set ", aFD, " ", *aFD,
			" IOContext: ", this, " ", *this );
		co_return false;
	}


	std::error_condition closeErr;
	auto sysCloseRet = co_await CloseOperation(*aFD, &closeErr);
	if(sysCloseRet == -1)
	{
		/* As per close(2) manual even when failing the file descriptor will be
		 * closed anyway, even if programatically dealt upstream it seems good
		 * to print a noisy report as this should anyway not happen */
		RS_ERR("failure closing ", *aFD, " ret: ", sysCloseRet, " ", closeErr);
		rs_error_bubble_or_exit(
			closeErr, errbub, "failure closing ", *aFD, " ", *this );
	}

	aFD->mFD = -1;
	mManagedFD.erase(fdIt);

	co_return sysCloseRet == 0;
}
