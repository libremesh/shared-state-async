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

#include <cstdio>
#include <fcntl.h>
#include <sys/types.h>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <csignal>
#include <sys/types.h>
#include <iterator>
#include <sstream>

#include "async_command.hh"
#include "io_context.hh"
#include "waitpid_operation.hh"
#include "read_operation.hh"
#include "write_operation.hh"

#include <util/rsdebug.h>
#include <util/rsdebuglevel1.h>

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434 /* System call # on most architectures */
#endif

static int pidfd_open(pid_t pid, unsigned int flags)
{
    return syscall(__NR_pidfd_open, pid, flags);
}

/*static*/ std::shared_ptr<AsyncCommand> AsyncCommand::execute(
        std::string cmd, IOContext& ioContext,
        std::error_condition* errbub )
{
	int parentToChildPipe[2]; // mFd_w
	int childToParentPipe[2]; // mFd_r
	auto& PARENT_READ  = childToParentPipe[0];
	auto& CHILD_WRITE  = childToParentPipe[1];
	auto& CHILD_READ   = parentToChildPipe[0];
	auto& PARENT_WRITE = parentToChildPipe[1];

	if (pipe(parentToChildPipe) == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "pipe(mFd_w) failed" );
		return nullptr;
	}

	if (pipe(childToParentPipe) == -1)
	{
		// Close the previously open pipe
		close(parentToChildPipe[1]);
		close(parentToChildPipe[0]);

		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "pipe(mFd_r) failed" );
		return nullptr;
	}

	pid_t forkRetVal = fork();
	if (forkRetVal == -1)
	{
		auto forkErrno = errno;

		// Close the previously opened pipes
		close(CHILD_READ);
		close(CHILD_WRITE);
		close(PARENT_READ);
		close(PARENT_WRITE);

		rs_error_bubble_or_exit(
		            rs_errno_to_condition(forkErrno), errbub,
		            "fork() failed" );
		return nullptr;
	}

	if( forkRetVal > 0)
	{
		int childWaitFD = pidfd_open(forkRetVal, 0);
		if(childWaitFD == -1)
		{
			auto pidfdOpenErrno = errno;

			// Close the previously opened pipes
			close(CHILD_READ);
			close(CHILD_WRITE);
			close(PARENT_READ);
			close(PARENT_WRITE);

			// Kill child process
			kill(forkRetVal, SIGKILL);

			rs_error_bubble_or_exit(
			            rs_errno_to_condition(pidfdOpenErrno), errbub,
			            "pidfd_open(...) failed" );
			return nullptr;
		}

		/* At this point graceful error handling becomes tricky, but I bet none
		 * of this functions should fail under non-dramatically pathological
		 * conditions so let's see what happens. If failure here still happens
		 * I am up to reading bug reports, and curious on how to reproduce that
		 * situation */

		auto tPac = ioContext.registerFD<AsyncCommand>(childWaitFD);
		ioContext.attach(tPac.get());

		tPac->mProcessId = forkRetVal;

		tPac->mStdOut = ioContext.registerFD(PARENT_READ);
		ioContext.attachReadonly(tPac->mStdOut.get());

		tPac->mStdIn = ioContext.registerFD(PARENT_WRITE);
		ioContext.attachWriteOnly(tPac->mStdIn.get());

		// Close child ends of the pipes
		close(CHILD_READ); close(CHILD_WRITE);

		return tPac;
	}

/* BEGIN: CODE EXECUTED ON THE CHILD PROCESS **********************************/
	if (forkRetVal == 0)
	{
		/* No need to keep those FD opened on the child */
		close(PARENT_WRITE);
		close(PARENT_READ);

		// Map child side of the pipe to child process standard input and output
		dup2(CHILD_READ,  STDIN_FILENO); close(CHILD_READ);
		dup2(CHILD_WRITE, STDOUT_FILENO); close(CHILD_WRITE);

		std::stringstream ss(cmd);
		std::istream_iterator<std::string> begin(ss);
		std::istream_iterator<std::string> end;
		std::vector<std::string> vstrings(begin, end);

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
			/* We are on the child process so no-one must be there to deal with
			 * the error condition bubbling up, terminate printing error
			 * details */
			RS_FATAL( rs_errno_to_condition(errno), " execvp failed for cmd: ",
			          cmd );
			print_stacktrace();
			exit(errno);
		}
	}
/* END: CODE EXECUTED ON THE CHILD PROCESS ************************************/

	// No one should get here, just make the code analizer happy
	return nullptr;
}

std::task<ssize_t> AsyncCommand::readStdOut(
        uint8_t* buffer, std::size_t len, std::error_condition* errbub)
{ co_return co_await asyncRead(*mStdOut, buffer, len, errbub); }

std::task<ssize_t> AsyncCommand::writeStdIn(
        const uint8_t* buffer, std::size_t len, std::error_condition* errbub )
{
	RS_DBG2( *mStdIn,
	         " buffer: ", reinterpret_cast<const void*>(buffer),
	         " len: ", len);
	RS_DBG4( " buffer content: ",
	         std::string(reinterpret_cast<const char*>(buffer), len) );

	co_return co_await asyncWrite(*mStdIn, buffer, len, errbub);
}

std::task<bool> AsyncCommand::closeStdIn(std::error_condition* errbub)
{
	auto mRet = co_await mIOContext.closeAFD(mStdIn, errbub);
	if(mRet) mStdIn.reset();
	co_return mRet;
}

std::task<bool> AsyncCommand::closeStdOut(std::error_condition* errbub)
{
	auto mRet = co_await mIOContext.closeAFD(mStdOut, errbub);
	if(mRet) mStdOut.reset();
	co_return mRet;
}

#if 0
bool PipedAsyncCommand::requestTermination(
        uint32_t timeoutSeconds, std::error_condition* errbub )
{
	/* TODO: Define proper error_conditions instead of lazily using std::errc */
	if(mProcessId == -1)
	{
		rs_error_bubble_or_exit(
		            std::errc::state_not_recoverable, errbub,
		            "attempt to terminate uninitialized process" );
		return false;
	}

	if(teminationTimeout)
	{
		rs_error_bubble_or_exit(
		            std::errc::timed_out, errbub,
		            "termination of: ", mProcessId, " already requested" );
		return false;
	}

	if(kill(mProcessId, SIGTERM) == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "kill failed sending SIGTERM to: ", mProcessId );
		return false;
	}

	teminationTimeout = time(NULL) + timeoutSeconds;
	return true;
}
#endif

/*static*/ std::task<bool> AsyncCommand::waitTermination(
        std::shared_ptr<AsyncCommand> pac,
        std::error_condition* errbub )
{
	auto tPid = pac->getPid();
	// TODO: save exit status to a member variable
	bool terminated =
	        tPid == co_await WaitpidOperation(*pac, nullptr, errbub);
	bool closed = terminated &&
			co_await pac->getIOContext().closeAFD(pac, errbub);
	co_return closed;
}

template<>
std::task<bool> IOContext::closeAFD(
        std::shared_ptr<AsyncCommand> aFD, std::error_condition* errbub )
{
	if(aFD->mStdIn)
		if(!co_await aFD->closeStdIn(errbub)) co_return false;

	if(aFD->mStdOut)
		if(!co_await aFD->closeStdOut(errbub)) co_return false;

	aFD->mProcessId = -1;

	co_return co_await closeAFD(
	            std::static_pointer_cast<AsyncFileDescriptor>(aFD), errbub );
}

std::ostream &operator<<(std::ostream& out, const AsyncCommand& aFD)
{
	return out << " aFD: " << &aFD << " FD: " << aFD.getFD()
	           << " mChildProcessId: " << aFD.getPid();
}
