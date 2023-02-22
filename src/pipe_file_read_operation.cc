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
#include "pipe_file_read_operation.hh"
#include <iostream>
#include "async_command.hh"

PipeFileReadOperation::PipeFileReadOperation(AsyncCommand* socket,
        void* buffer,
        std::size_t len)
    : BlockSyscall{}
    , socket{socket}
    , buffer_{buffer}
    , len_{len}
{
    socket->io_context_.watchRead(socket);
    RS_DBG0("")<< "socket_fileRead_operation created\n";
}

PipeFileReadOperation::~PipeFileReadOperation()
{
    socket->io_context_.unwatchRead(socket);
    RS_DBG0("")<< "~socket_fileRead_operation\n";
}

ssize_t PipeFileReadOperation::syscall()
{
    std::string result;
    RS_DBG0("")<< "fgets(" << fileno(socket->pipe) << ", buffer_, len_, 0)\n";
    while (!feof(socket->pipe))
    {
        if (fgets((char *)buffer_, len_, socket->pipe) != nullptr)
            result += (char *)buffer_;
    }
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());

    return 10;
    //todo: fix this
}

void PipeFileReadOperation::suspend()
{
    socket->coroRecv_ = awaitingCoroutine_;
}
