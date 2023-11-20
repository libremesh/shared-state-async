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
#pragma once

#include <memory>
#include <string>
#include <ctime>

#include "async_file_descriptor.hh"
#include "io_context.hh"
#include "file_read_operation.hh"
#include "file_write_operation.hh"

/**
 * @brief Async command execution and control
 * 
 * This implementation is fully async supporting async reading, writing,
 * waiting for the child process to terminate and reading exit status.
 *
 * TODO: implement a way to return process exit code
 */
class AsyncCommand : public AsyncFileDescriptor
{
public:
	/**
	 * Start execution of a command.
	 *
	 * @param cmd command to be executed asynchronously. This parameter is
	 * passed by value, it wont be a large string and it is a secure way to send
	 * the parameter for detachable coroutines in multithread scenarios.
	 * @param ioContext IO context that will deal with input and output
	 * operations.
	 * @param errbub optional storage for error details, to deal more gracefully
	 * with them on the caller side, if null the program will be terminated on
	 * error.
	 * @return nullptr on failure, pointer to the command handle on success
	 *
	 * @warning remember to call @see waitForProcessTermination after using the
	 * object to prevent zombi process creation.
	 * More interesting insights/explanations might be found at
	 * http://unixwiz.net/techtips/remap-pipe-fds.html
	 */
	static std::shared_ptr<AsyncCommand> execute(
	        std::string cmd, IOContext& ioContext,
	        std::error_condition* errbub = nullptr );

	/**
	 * Asynchronously waits for a process to die.
	 * @warning if this method is not called the forked process will be
	 * a zombi.
	 * TODO: add runtime consistenct check in the destructor to detect orfaned
	 * process
	 * @return false on error, true otherwise
	 */
	static std::task<bool> waitTermination(
	        std::shared_ptr<AsyncCommand> pac,
	        std::error_condition* errbub = nullptr );

	AsyncCommand(const AsyncCommand &) = delete;
	AsyncCommand() = delete;
	~AsyncCommand()
	{
		/* In this design we can use destructor to detect logic errors at
		 * runtime */

		RS_DBG1(*this);
		if(mProcessId != -1)
		{
			RS_FATAL( *this,
			          " Destructor called before IOContext::closeAFD "
			          "report to developers!" );
			print_stacktrace();
			exit(static_cast<int>(std::errc::state_not_recoverable));
		}
	};

	std::task<ssize_t> readStdOut(
	        uint8_t* buffer, std::size_t len,
	        std::error_condition* errbub = nullptr );
	std::task<ssize_t> writeStdIn(
	        const uint8_t *buffer, std::size_t len,
	        std::error_condition* errbub = nullptr );
	inline pid_t getPid() const { return mProcessId; }

	std::task<bool> closeStdIn(std::error_condition* errbub = nullptr);
	std::task<bool> closeStdOut(std::error_condition* errbub = nullptr);

protected:
	friend IOContext;
	AsyncCommand(int fd, IOContext &ioContext):
	    AsyncFileDescriptor(fd, ioContext) {}

	pid_t mProcessId = -1;
	std::shared_ptr<AsyncFileDescriptor> mStdOut = nullptr;
	std::shared_ptr<AsyncFileDescriptor> mStdIn = nullptr;
	std::shared_ptr<AsyncFileDescriptor> mWaitFD = nullptr;

#if 0
	static constexpr uint32_t DEFAULT_TERMINATION_TIMEOUT_SECONDS = 60;
	bool requestTermination(
	        uint32_t timeoutSeconds = DEFAULT_TERMINATION_TIMEOUT_SECONDS,
	        std::error_condition* errbub = nullptr );
	time_t teminationTimeout = 0;
#endif
};

template<>
std::task<bool> IOContext::closeAFD(
        std::shared_ptr<AsyncCommand> aFD, std::error_condition* errbub );

std::ostream &operator<<(std::ostream& out, const AsyncCommand& aFD);
