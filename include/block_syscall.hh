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

#include "debug/rsdebuglevel2.h"


/**
 * @brief BlockSyscall is a base class for all kind of asynchronous syscalls
 * @tparam SyscallOpt child class, passed as template paramether to avoid
 *	dynamic dispatch performance hit
 * @tparam ReturnValue type of the return value of the real syscall
 * The costructor take a pointer to a std::error_condition to deal with errors,
 * if nullptr is passed it is assumed that the upstream caller won't deal with
 * the error so the program will exit after printing where the error happened.
 * On the contrary if a valid pointer is passed, when an error occurr the
 * information about the error will be stored there for the upstream caller to
 * deal with it.
 *
 * Derived classes MUST implement the following methods
 * @code{.cpp}
 * ReturnValue syscall();
 * void suspend();
 * @endcode
 *
 * The syscall method is where the actuall syscall must happen.
 *
 * TODO: What is the suspend method supposed to do?
 *
 * Pure virtual definition not included at moment to virtual method tables
 * generation (haven't verified if this is necessary or not).
 */
template <typename SyscallOpt, typename ReturnValue>
class BlockSyscall // Awaiter
{
public:
	BlockSyscall(std::error_condition* ec): mHaveSuspend{false}, mError{ec} {}

	bool await_ready() const noexcept
	{
		RS_DBG0("");
		return false;
	}

    bool await_suspend(std::coroutine_handle<> awaitingCoroutine)
    {
        RS_DBG0("await_suspend");

        static_assert(std::is_base_of_v<BlockSyscall, SyscallOpt>);
		mAwaitingCoroutine = awaitingCoroutine;
		mReturnValue = static_cast<SyscallOpt *>(this)->syscall();
		mHaveSuspend =
		    mReturnValue == -1 && (
		            errno == EAGAIN ||
		            errno == EWOULDBLOCK ||
		            errno == EINPROGRESS );
		if (mHaveSuspend)
        {
            /// haveSuspend_ true returns control to the caller/resumer of the current coroutine
            RS_WARN("...suspendiendo ... por un -1");
            static_cast<SyscallOpt *>(this)->suspend();
        }
		else if (mReturnValue == -1)
		{
			/* haveSuspend_ false but returnValue -1 resumes the current
			 * coroutine.
			 * But the system call has failed.. the caller has to be notified */
			rs_error_bubble_or_exit(
			            rs_errno_to_condition(errno), mError,
			            "A syscall has failed" );
		}
        // the haveSuspend_ false resumes the current coroutine. (doesn't suspend)
		return mHaveSuspend;
    }

	ReturnValue await_resume()
	{
		RS_DBG3("");

		if (mHaveSuspend)
			mReturnValue = static_cast<SyscallOpt *>(this)->syscall();

		return mReturnValue;
	}

protected:
	bool mHaveSuspend;
	std::coroutine_handle<> mAwaitingCoroutine;

	std::error_condition* mError;
	ReturnValue mReturnValue;
};
