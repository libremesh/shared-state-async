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
#include "dying_process_wait_operation.hh"
#include <iostream>
#include <unistd.h>
#include "async_command.hh"
#include <sys/wait.h>

DyingProcessWaitOperation::DyingProcessWaitOperation(std::shared_ptr<AsyncFileDescriptor> socket, pid_t process_to_wait)
    : BlockSyscall{}, socket{socket}
{
    socket->io_context_.watchRead(socket.get());
    pid = process_to_wait;
    RS_DBG0("FileWriteOperation created\n");
}

DyingProcessWaitOperation::~DyingProcessWaitOperation()
{
    socket->io_context_.unwatchRead(socket.get());
    RS_DBG0("~FileWriteOperation\n");
}

pid_t DyingProcessWaitOperation::syscall()
{
    pid_t cpid = waitpid(pid, NULL, WNOHANG);
    RS_DBG0("wait returned ", cpid ,"errno ", errno);
    if (cpid == 0 || cpid == -1)
    {
        // just in case kill the process.
        kill(pid, SIGKILL);
        cpid = -1; // if the state has not changed wait returns 0...but corrutine expects -1
    }
    else if (cpid == pid)
    {
        RS_DBG0("Successful wait returned process id", cpid);
    }
    return cpid;
}

void DyingProcessWaitOperation::suspend()
{
    RS_DBG0(__PRETTY_FUNCTION__ );
    socket->coroRecv_ = awaitingCoroutine_;
}
