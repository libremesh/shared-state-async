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

#include "dying_process_wait_operation.hh"
#include "async_file_descriptor.hh"
#include "io_context.hh"

#include <unistd.h>
#include <sys/wait.h>

#include <util/rsdebuglevel2.h>


/** @brief This blocking operation waits for a child process that has 
 *  already done his job. It also kills the process in case it has not died yet. 
 *  @warning Call this method after you really want the process to die. If
 *  the process is not dead the method will kill it. 
 */
DyingProcessWaitOperation::DyingProcessWaitOperation(
        AsyncFileDescriptor& afd,
        pid_t process_to_wait,
        std::error_condition* ec ):
    AwaitableSyscall{afd, ec}, mPid(process_to_wait)
{
	mAFD.getIOContext().watchRead(&mAFD);
}

DyingProcessWaitOperation::~DyingProcessWaitOperation()
{
	mAFD.getIOContext().unwatchRead(&mAFD);
}

pid_t DyingProcessWaitOperation::syscall()
{
	pid_t cpid = waitpid(mPid, NULL, WNOHANG);

	if (cpid == 0 || cpid == -1)
	{
		/* TODO: kill is very harsh and may break stuff, and not what should be
		 * done here.
		 * We should instead implement some kind of timeout mechanism and in any
		 * case after SIGTERM usage as a PipedAsyncCommand method */
		kill(mPid, SIGKILL);

		/* if the state has not changed wait returns 0...
		 * but blocksyscall expects -1 */
		cpid = -1;
		errno = EAGAIN;
	}
	else if (cpid == mPid)
	{
		RS_DBG2( "Success waiting process id: ", cpid, " ", mAFD );
	}
	return cpid;
}
