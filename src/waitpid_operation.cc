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

#include "waitpid_operation.hh"
#include "async_file_descriptor.hh"
#include "io_context.hh"
#include "async_command.hh"

#include <unistd.h>
#include <sys/wait.h>

#include <util/rsdebuglevel1.h>

/** @brief This awaitable operation waits for a child process that has
 *  already done his job. */
WaitpidOperation::WaitpidOperation(
        AsyncCommand& afd,
        int* wstatus,
        std::error_condition* ec ):
    AwaitableSyscall{afd, ec}, mWstatus(wstatus)
{
	mAFD.getIOContext().watchRead(&mAFD);
}

WaitpidOperation::~WaitpidOperation()
{
	mAFD.getIOContext().unwatchRead(&mAFD);
}

pid_t WaitpidOperation::childPid() const
{
	return static_cast<AsyncCommand&>(mAFD).getPid();
}

pid_t WaitpidOperation::syscall()
{
	pid_t mChildPid = childPid();
	pid_t waitPidRet = waitpid(mChildPid, mWstatus, WNOHANG);

	if(waitPidRet == 0)
	{
		/* Process hasn't terminated yet, AwaitableSyscall expect -1 + EAGAIN */
		errno = EAGAIN;
		return -1;
	}

#if RS_DEBUG_LEVEL > 1
	if (waitPidRet == mChildPid)
		RS_DBG( "Success waiting process id: ", waitPidRet, " ", mAFD );
#endif //  RS_DEBUG_LEVEL > 1

	return waitPidRet;
}
