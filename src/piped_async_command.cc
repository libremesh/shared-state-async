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

std::error_condition PipedAsyncCommand::init(std::string cmd, IOContext &context)
{
    RS_DBG0("") << "PipedAsyncCommand construction " << cmd << std::endl;
    //      parent        child
    //      fd1[1]        fd1[0]
    //        4 -- fd_w --> 3
    //      fd2[0]        fd2[1]
    //        5 <-- fd_r -- 6
    if (pipe(fd_w) == -1)
    {
        RS_FATAL("open pipe failed");
        return rs_errno_to_condition(errno);
    }
    if (pipe(fd_r) == -1)
    {
        RS_FATAL("open pipe failed");
        return rs_errno_to_condition(errno);
    }
    async_read_end_fd = std::make_shared<AsyncFileDescriptor>(fd_r[0], context);
    context.attachReadonly(async_read_end_fd.get());
    async_write_end_fd = std::make_shared<AsyncFileDescriptor>(fd_w[1], context);
    context.attachWriteOnly(async_write_end_fd.get());
    pid_t proces_id = fork();
    RS_DBG0("") << "forked process ---- " << proces_id << "........................... " << std::endl;
    if (proces_id == -1)
    {
        RS_FATAL("failed to fork the process");
        return rs_errno_to_condition(errno);
    }
    if (proces_id == 0)
    { /* Child reads from pipe and writes back as soon as it finishes*/

        close(fd_w[1]);      // Close writing end of first pipe
        close(STDIN_FILENO); // closing stdin
        dup(fd_w[0]);        // replacing stdin with pipe read

        // Close both reading ends
        close(fd_w[0]);
        close(fd_r[0]);

        close(STDOUT_FILENO); // closing stdout
        dup(fd_r[1]);         // replacing stdout with pipe write
        close(fd_r[1]);

        // emulate an echo command by reading the contents of the pipe using cat
        std::vector<char *> argc;
        argc.emplace_back(const_cast<char *>(cmd.data()));
        // NULL terminate
        argc.push_back(nullptr);
        // The first argument to execvp should be the same as the
        // first element in argc
        execvp(argc.data()[0], argc.data());
        perror("execvp of \"cat\" failed");
        exit(1);
    }
    forked_proces_id = proces_id;
    int pid_fd = pidfd_open(proces_id, 0);
    if (pid_fd == -1)
    {
        perror("pidfd_open");
        exit(EXIT_FAILURE);
    }
    async_process_wait_fd = std::make_shared<AsyncFileDescriptor>(pid_fd, context);
    context.attachReadonly(async_process_wait_fd.get());
    
    close(fd_r[1]);
    close(fd_w[0]);
    RS_DBG0("") << "PipedAsyncCommand creation finished " << std::endl;
    return std::make_error_condition(std::errc());
}

PipedAsyncCommand::~PipedAsyncCommand()
{
    async_read_end_fd.reset();
    async_write_end_fd.reset();
    async_process_wait_fd.reset();
    
}

FileReadOperation PipedAsyncCommand::readpipe(void *buffer, std::size_t len)
{
    return FileReadOperation{async_read_end_fd, buffer, len};
}

FileWriteOperation PipedAsyncCommand::writepipe(void *buffer, std::size_t len)
{
    return FileWriteOperation{async_write_end_fd, buffer, len};
}

DyingProcessWaitOperation PipedAsyncCommand::whaitforprocesstodie()
{
    return DyingProcessWaitOperation{async_process_wait_fd,forked_proces_id};
}
