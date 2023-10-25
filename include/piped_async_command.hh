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

#include "async_file_desc.hh"
#include "io_context.hh"
#include "file_read_operation.hh"
#include "file_write_operation.hh"
#include "dying_process_wait_operation.hh"
#include "socket.hh"

/**
 * @brief AsyncCommand implementation using fork excec and dual pipes
 * 
 * This implementation is fully async supporting async reading, writing and
 * waiting for the child process to die.
 *
 * TODO: implement a way to return process exit code
 */
class PipedAsyncCommand
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
	static std::unique_ptr<PipedAsyncCommand> execute(
	        std::string cmd, IOContext& ioContext,
	        std::error_condition* errbub = nullptr );

	~PipedAsyncCommand();

	ReadOp readpipe(uint8_t* buffer, std::size_t len);
    FileWriteOperation writepipe(const uint8_t *buffer, std::size_t len);
	DyingProcessWaitOperation waitForProcessTermination();
    void finishwriting();
    void finishReading();
    bool doneReading();


private:
	PipedAsyncCommand(const PipedAsyncCommand &) = delete;
	PipedAsyncCommand() = default;

	// we need two file descriptors to interact with the forked process
	//      parent        child
	//      fd1[1]        fd1[0]
	//        4 -- mfd_W --> 3
	//      fd2[0]        fd2[1]
	//        5 <-- mFd_r -- 6
	// TODO: use proper naming and mapping @see execute code
    int mFd_w[2];
    int mFd_r[2];
    pid_t forked_proces_id = -1;

	// TODO: are shared_ptr really needed here? Check if unique_ptr is enough
    std::shared_ptr<AsyncFileDescriptor> async_read_end_fd;
    std::shared_ptr<AsyncFileDescriptor> async_write_end_fd;
    std::shared_ptr<AsyncFileDescriptor> async_process_wait_fd;

	friend ReadOp;
	friend AsyncFileDescriptor;
	friend Socket;
};
