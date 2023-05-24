/*
 * Shared State
 *
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

#include <fcntl.h>
#include <sys/types.h>
#include <iostream>
#include <fcntl.h> /* Obtain O_* constant definitions */
#include <unistd.h>
#include <vector>
#include "piped_async_command.hh"
#include "io_context.hh"
#include <signal.h>
#include <sys/types.h>
#include <iterator>
#include <sstream>

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434 /* System call # on most architectures */
#endif

static int
pidfd_open(pid_t pid, unsigned int flags)
{
    return syscall(__NR_pidfd_open, pid, flags);
}

PipedAsyncCommand::PipedAsyncCommand()
{
}

/**
 * Initializes the object
 *
 * This is a factory method it must be called for every object.
 * @warning remember to call "whaitforprocesstodie" after using the object to
 * prevent zombi process creation.
 *
 * @param cmd a copy of the command to be executed asynchronously. This
 * parameter is an explicit copy, it wont be a large string and it is a secure
 * way to send the parameter for detachable coroutines.
 * @return error_condition indicating success or the cause of the initialization
 * failure.
 */
std::error_condition PipedAsyncCommand::init(std::string cmd, IOContext &context)
{
    RS_DBG0("PipedAsyncCommand construction ", cmd);
    //      parent        child
    //      fd1[1]        fd1[0]
    //        4 -- fd_w --> 3
    //      fd2[0]        fd2[1]
    //        5 <-- mFd_r -- 6
    if (pipe(mFd_w) == -1)
    {
        RS_FATAL("open pipe failed");
        return rs_errno_to_condition(errno);
    }
    if (pipe(mFd_r) == -1)
    {
        RS_FATAL("open pipe failed");
        return rs_errno_to_condition(errno);
    }
    async_read_end_fd = std::make_shared<AsyncFileDescriptor>(mFd_r[0], context);
    context.attachReadonly(async_read_end_fd.get());
    async_write_end_fd = std::make_shared<AsyncFileDescriptor>(mFd_w[1], context);
    context.attachWriteOnly(async_write_end_fd.get());
    pid_t process_id = fork();
    RS_DBG0("forked process ---- ", process_id, "........................... ");
    if (process_id == -1)
    {
        RS_ERR("failed to fork the process");
        return rs_errno_to_condition(errno);
    }
    if (process_id == 0)
    {                        /* Child reads from pipe and writes back as soon as it finishes*/
        close(mFd_w[1]);     /// Close writing end of first pipe
        close(STDIN_FILENO); /// closing stdin
        dup(mFd_w[0]);       /// replacing stdin with pipe read

        /// Close both reading ends
        close(mFd_w[0]);
        close(mFd_r[0]);

        close(STDOUT_FILENO); /// closing stdout
        dup(mFd_r[1]);        /// replacing stdout with pipe write
        close(mFd_r[1]);

        std::stringstream ss(cmd);
        std::istream_iterator<std::string> begin(ss);
        std::istream_iterator<std::string> end;
        std::vector<std::string> vstrings(begin, end);
        std::copy(vstrings.begin(), vstrings.end(), std::ostream_iterator<std::string>(std::cout, "\n"));

        std::vector<char *> argcexec(vstrings.size(), nullptr);
        for (int i = 0; i < vstrings.size(); i++)
        {
            argcexec[i] = vstrings[i].data();
        }
        /// NULL terminate the command line
        argcexec.push_back(nullptr);
        // The first argument to execvp should be the same as the
        // first element in argc
        execvp(argcexec.data()[0], argcexec.data());
        RS_FATAL("* * * * * * * execvp failed ", argcexec.data());
        return rs_errno_to_condition(errno);
    }
    forked_proces_id = process_id;
    int pid_fd = pidfd_open(forked_proces_id, 0);
    if (pid_fd == -1)
    {
        RS_ERR("pidfd_open failed, you wont be able to wait for the dying process");
        return rs_errno_to_condition(errno);
    }
    async_process_wait_fd = std::make_shared<AsyncFileDescriptor>(pid_fd, context);
    context.attachReadonly(async_process_wait_fd.get());

    close(mFd_r[1]);
    close(mFd_w[0]);
    RS_DBG0("PipedAsyncCommand creation finished ");
    return std::error_condition();
}

PipedAsyncCommand::~PipedAsyncCommand()
{
    async_read_end_fd.reset();
    async_write_end_fd.reset();
    async_process_wait_fd.reset();
}

FileReadOperation PipedAsyncCommand::readpipe(uint8_t *buffer, std::size_t len)
{
    return FileReadOperation{async_read_end_fd, buffer, len};
}

FileWriteOperation PipedAsyncCommand::writepipe(const uint8_t *buffer, std::size_t len)
{
    return FileWriteOperation{async_write_end_fd, buffer, len};
}
void PipedAsyncCommand::finishwriting()
{
    async_write_end_fd.reset();
}

/**
 * Asynchronously waits for a process to die.
 * @warning if this method is not called the forked process will be
 * a zombi.
 */
DyingProcessWaitOperation PipedAsyncCommand::whaitforprocesstodie()
{
    return DyingProcessWaitOperation{async_process_wait_fd, forked_proces_id};
}
