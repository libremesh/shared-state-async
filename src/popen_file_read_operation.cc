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

#include <iostream>

#include "popen_file_read_operation.hh"
#include "popen_async_command.hh"

PopenFileReadOperation::PopenFileReadOperation(PopenAsyncCommand* socket,
        uint8_t* buffer,
        std::size_t len, std::shared_ptr<std::error_condition> ec)
    :BlockSyscall{ec}
    , socket{socket}
    , mBuffer_{buffer}
    , len_{len}
{
    socket->io_context_.watchRead(socket);
    RS_DBG0("socket_fileRead_operation created\n");
}

PopenFileReadOperation::~PopenFileReadOperation()
{
    socket->io_context_.unwatchRead(socket);
    RS_DBG0("~socket_fileRead_operation\n");
}

ssize_t PopenFileReadOperation::syscall()
{
    std::string result;
    RS_DBG0("fgets(" , fileno(socket->mPipe) );
    while (!feof(socket->mPipe))
    {
        if (fgets((char *)mBuffer_, len_, socket->mPipe) != nullptr)
            result += (char *)mBuffer_;
    }
//    result.erase(std::remove(result.begin(), result.end(), '\n'), result.cend());
    return 10;
}

void PopenFileReadOperation::suspend()
{
	RS_DBG3("");
	socket->coroRecv_ = mAwaitingCoroutine;
}
