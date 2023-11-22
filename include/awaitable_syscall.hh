/*
 * Shared State
 *
 * Copyright (c) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include <cerrno>
#include <coroutine>
#include <memory>
#include <type_traits>
#include <iostream>

#include "async_file_descriptor.hh"

#include <util/rserrorbubbleorexit.h>
#include <util/rsdebug.h>
#include <util/rsdebuglevel2.h>

/**
 * @brief AwaitableSyscall is a base class for all kind of asynchronous syscalls
 * @tparam SyscallOpt child class, passed as template paramether to avoid
 * dynamic dispatch performance hit
 * @tparam ReturnValue type of the return value of the real syscall
 * The costructor take a pointer to a std::error_condition to deal with errors,
 * if nullptr is passed it is assumed that the upstream caller won't deal with
 * the error so the program will exit after printing where the error happened.
 * On the contrary if a valid pointer is passed, when an error occurr the
 * information about the error will be stored there for the upstream caller to
 * deal with it.
 * @tparam multiShot true if wrapped syscall may cause epoll_wait to return more
 * then once before being ready/complete, waitpid is an example of that.
 * @tparam errorValue customize value that represent failure returned bt syscall
 *
 * Derived classes MUST pass themselves as SyscallOpt and implement the
 * following method:
 * @code{.cpp}
 * ReturnValue syscall();
 * @endcode
 *
 * The syscall method is where the actuall syscall must happen, on failure
 * errorValue must be returned, if more attempts are needed errno must be set to
 * EAGAIN @see shouldWait() for other errno values interpreted like EAGAIN
 */
template < typename SyscallOp,
           typename ReturnType,
           bool multiShot = false,
           ReturnType errorValue = -1 >
class AwaitableSyscall
{
public:
		AwaitableSyscall( AsyncFileDescriptor& afd,
										std::error_condition* ec = nullptr ):
				mError{ec}, mAFD(afd)
	{
		/* Put static checks here and not in template class scope to avoid
		 * invalid use of imcomplete type xxxOperation compiler errors */
		static_assert(std::is_base_of<AwaitableSyscall, SyscallOp>::value);
		static_assert(requires(SyscallOp& op) { op.syscall(); });
	}

	bool await_ready() const noexcept
	{
		RS_DBG3(mAFD);
		return false;
	}

	bool await_suspend(std::coroutine_handle<> awaitingCoroutine)
	{
		mAwaitingCoroutine = awaitingCoroutine;
		mReturnValue = static_cast<SyscallOp*>(this)->syscall();

		RS_DBG2( mAFD,
				" shouldWait(): ", shouldWait(mReturnValue, errno),
				" mReturnValue: ", mReturnValue,
				" ", rs_errno_to_condition(errno) );

		if(shouldWait(mReturnValue, errno))
		{
			/* The syscall indicated we must wait, and retry later so let's
			 * suspend to return the control to the caller and be resumed later
			 */
			suspend();
			return true;
		}

		if(mReturnValue == errorValue)
		{
			/* The syscall failed for other reason let's notify the caller if
			 * possible or close the program printing an error */

			RS_DBG2( "syscall failed with ret: ", mReturnValue,
					 " errno: ", rs_errno_to_condition(errno) );
			rs_error_bubble_or_exit(
				rs_errno_to_condition(errno), mError,
				" syscall failed" );

			/* If downstream callers apparently get an error before crashing,
			 * but print errno 0, the reason is NEVER the failed syscall
			 * that forgot to set it but some null/dangling pointer that bubble
			 * up, due to a missing check, undetected in the call stack, so when
			 * an error is finally printed  the errno value it is getting got
			 * most likely borked at some point, and is not the original from
			 * the syscall */
		}

		/* Instantaneous success or hard failure must not suspend at all */
		return false;
	}

	ReturnType await_resume()
	{
		if(!mDidSuspend)
		{
			/* Operation was completed on first attempt, without suspending,
			 * just return the result */
			RS_DBG2( mAFD, " completed at firts attempt mReturnValue: ",
					mReturnValue );
			return mReturnValue;
		}

		// We had to suspend last time, so we need to attempt the syscall again
		mReturnValue = static_cast<SyscallOp *>(this)->syscall();
		RS_DBG2(mAFD, " syscall returns: ", mReturnValue );

		if(multiShot && shouldWait(mReturnValue, errno))
		{
			RS_DBG1( "syscall want more waiting on resume ",
					 "mReturnValue: ", mReturnValue,
					 rs_errno_to_condition(errno) );
			suspend();
		}
		else if(mReturnValue == errorValue)
		{
			RS_DBG1( "syscall failed on resume ",
					 "mReturnValue: ", mReturnValue,
					 rs_errno_to_condition(errno) );
			rs_error_bubble_or_exit(
				rs_errno_to_condition(errno), mError,
				"syscall failed on resume" );
		}

		return mReturnValue;
	}

	void suspend()
	{
		RS_DBG2(mAFD, " ", mAwaitingCoroutine.address());
		mDidSuspend = true;
		mAFD.addPendingOp(mAwaitingCoroutine);
	}

	/**
	 * @brief errno tell we should wait or not?
	 * @param sErrno errno as set by the previous syscall
	 * @return true if syscall told we should wait false otherwise
	 */
	static bool shouldWait(ReturnType retval, int sErrno)
	{
		return (retval == errorValue) && ( errno == EAGAIN ||
										   errno == EWOULDBLOCK ||
										   errno == EINPROGRESS );
	}

private:
	bool mDidSuspend = false;
	std::coroutine_handle<> mAwaitingCoroutine;

	std::error_condition* const mError = nullptr;
	ReturnType mReturnValue = errorValue;

protected:
	AsyncFileDescriptor& mAFD;
};
