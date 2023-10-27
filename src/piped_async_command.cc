/*
 * Shared State
 *
 * Copyright (C) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
 * Copyright (C) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (C) 2023  Instituto Nacional de Tecnología Industrial
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

#include <fcntl.h>
#include <sys/types.h>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <signal.h>
#include <sys/types.h>
#include <iterator>
#include <sstream>

#include "piped_async_command.hh"
#include "io_context.hh"

#include <util/rsdebug.h>
#include <util/rsdebuglevel2.h>

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434 /* System call # on most architectures */
#endif

static int pidfd_open(pid_t pid, unsigned int flags)
{
    return syscall(__NR_pidfd_open, pid, flags);
}

/*static*/ std::unique_ptr<PipedAsyncCommand> PipedAsyncCommand::execute(
        std::string cmd, IOContext& ioContext,
        std::error_condition* errbub )
{
	std::unique_ptr<PipedAsyncCommand> pCmd(new PipedAsyncCommand);

	if (pipe(pCmd->mFd_w) == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "pipe(mFd_w) failed" );
		return nullptr;
	}

	if (pipe(pCmd->mFd_r) == -1)
	{
		// Close the previously open pipe
		close(pCmd->mFd_w[1]);
		close(pCmd->mFd_w[0]);

		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "pipe(mFd_r) failed" );
		return nullptr;
	}

	/* TODO: double check correct naming and them move to class declaration */
	auto& PARENT_READ  = pCmd->mFd_r[0];
	auto& CHILD_WRITE  = pCmd->mFd_r[1];
	auto& CHILD_READ   = pCmd->mFd_w[0];
	auto& PARENT_WRITE = pCmd->mFd_w[1];

	pCmd->async_read_end_fd =
	        std::make_shared<AsyncFileDescriptor>(PARENT_READ, ioContext);
	ioContext.attachReadonly(pCmd->async_read_end_fd.get());

	pCmd->async_write_end_fd =
	        std::make_shared<AsyncFileDescriptor>(PARENT_WRITE, ioContext);
	ioContext.attachWriteOnly(pCmd->async_write_end_fd.get());

	pid_t process_id = fork();
	RS_DBG2("forked process id: ", process_id);

	if (process_id == -1)
	{
		// Close the previously open pipe
		close(CHILD_READ);
		close(CHILD_WRITE);
		pCmd->async_read_end_fd.reset();
		pCmd->async_write_end_fd.reset();

		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "fork() failed" );
		return nullptr;
	}

	if (process_id == 0)
	{
		/* Child reads from pipe and writes back as soon as it finishes */

		close(PARENT_WRITE);
		close(PARENT_READ);

		dup2(CHILD_READ,  0);  close(CHILD_READ);
		dup2(CHILD_WRITE, 1);  close(CHILD_WRITE);


		std::stringstream ss(cmd);
		std::istream_iterator<std::string> begin(ss);
		std::istream_iterator<std::string> end;
		std::vector<std::string> vstrings(begin, end);
		std::copy( vstrings.begin(), vstrings.end(),
		           std::ostream_iterator<std::string>(std::cout, "\n") );

		std::vector<char *> argcexec(vstrings.size(), nullptr);
		for (int i = 0; i < vstrings.size(); i++)
		{
			argcexec[i] = vstrings[i].data();
		}
		argcexec.push_back(nullptr); // NULL terminate the command line

		/* The first argument to execvp should be the same as the first element
		 * in argc */
		if(execvp(argcexec.data()[0], argcexec.data()) == -1)
		{
			rs_error_bubble_or_exit(
			            rs_errno_to_condition(errno), errbub,
			            "execvp(...) failed" );
			return nullptr;
		}
	}

	pCmd->forked_proces_id = process_id;
	int pid_fd = pidfd_open(pCmd->forked_proces_id, 0);
	if (pid_fd == -1)
	{
		// wont be able to wait for the dying process
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "pidfd_open(...) failed" );
		return nullptr;
	}

	pCmd->async_process_wait_fd =
	        std::make_shared<AsyncFileDescriptor>(pid_fd, ioContext);
	ioContext.attachReadonly(pCmd->async_process_wait_fd.get());

	close(CHILD_READ);
	close(CHILD_WRITE);

	return pCmd;
}

PipedAsyncCommand::~PipedAsyncCommand()
{
	async_read_end_fd.reset();
	//async_write_end_fd.reset();
	async_process_wait_fd.reset();
}

ReadOp PipedAsyncCommand::readpipe(uint8_t *buffer, std::size_t len)
{
	return ReadOp{async_read_end_fd, buffer, len};
}

FileWriteOperation PipedAsyncCommand::writepipe(
        const uint8_t* buffer, std::size_t len )
{
	return FileWriteOperation{*async_write_end_fd, buffer, len};
}
void PipedAsyncCommand::finishwriting()
{
	RS_DBG0("async_write_end_fd.use_count() ",  async_write_end_fd.use_count());
    async_write_end_fd.get()->io_context_.unwatchWrite(async_write_end_fd.get());
    async_write_end_fd.reset();
    //close(PARENT_WRITE); //no funciona
}

/**
 * When the filedescriptor is closed but it notifies with a read event the 
 * file descriptor is marked as done, to be able to stop reading
*/
bool PipedAsyncCommand::doneReading()
{
    return async_read_end_fd.get()->doneRecv_;
}

void PipedAsyncCommand::finishReading()
{
	RS_DBG0("async_read_end_fd.use_count() ",  async_read_end_fd.use_count());
    async_read_end_fd.get()->io_context_.unwatchRead(async_read_end_fd.get());
    async_read_end_fd.reset();
}

/**
 * Asynchronously waits for a process to die.
 * @warning if this method is not called the forked process will be
 * a zombi.
 */
DyingProcessWaitOperation PipedAsyncCommand::waitForProcessTermination()
{
	return DyingProcessWaitOperation(
	            *async_process_wait_fd.get(),
	            forked_proces_id );
}
