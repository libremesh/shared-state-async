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
#include "file_write_operation.hh"
#include <iostream>
#include <unistd.h>
#include "async_file_desc.hh"

FileWriteOperation::FileWriteOperation(std::shared_ptr<AsyncFileDescriptor> socket,
                                       const uint8_t *buffer,
                                       std::size_t len, std::shared_ptr<std::error_condition> ec)
    :BlockSyscall{ec}, socket{socket}, buffer_{buffer}, len_{len}
{
    socket->io_context_.watchWrite(socket.get());
    RS_DBG0("FileWriteOperation created\n");
}

FileWriteOperation::~FileWriteOperation()
{
    socket->io_context_.unwatchWrite(socket.get());
    RS_DBG0("~FileWriteOperation\n");
}

ssize_t FileWriteOperation::syscall()
{
    RS_DBG0("FileWriteOperation write(", socket->fd_, ",", (char *)buffer_, ",", len_, ")\n");
    ssize_t bytes_writen = write(socket->fd_, (char *)buffer_, len_);
    if (bytes_writen == -1)
    {
        RS_ERR("**** error ****", strerror(errno));
    }
    RS_DBG0("bytes_writen", bytes_writen);
    return bytes_writen;
}

void FileWriteOperation::suspend()
{
    RS_DBG0(__PRETTY_FUNCTION__);

    socket->coroSend_ = awaitingCoroutine_;
}
