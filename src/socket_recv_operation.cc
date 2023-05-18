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
#include "socket_recv_operation.hh"
#include <iostream>
#include "socket.hh"

SocketRecvOperation::SocketRecvOperation(Socket *socket,
                                         uint8_t *buffer,
                                         std::size_t len,
                                         std::shared_ptr<std::error_condition> ec )
    : BlockSyscall{ec}, socket{socket}, mBuffer_{buffer}, len_{len}
{
    socket->io_context_.watchRead(socket);
    RS_DBG0("socket_recv_operation\n)");
}

SocketRecvOperation::~SocketRecvOperation()
{
    socket->io_context_.unwatchRead(socket);
    RS_DBG0("~socket_recv_operation\n");
}

ssize_t SocketRecvOperation::syscall()
{
    RS_DBG0("recv(", socket->fd_, "content", (char *)mBuffer_, "ammount", len_, ")\n");
    ssize_t bytesread = recv(socket->fd_, mBuffer_, len_, 0);
    /* this method is invoked at least once but the socket is not free.
     * this is not problem since the BlockSyscall::await_suspend will test for -1 return value and test errno (EWOULDBLOCK or EAGAIN)
     * and then suspend the execution until a new notification arrives
     */
    if (bytesread == -1)
    {
        RS_WARN("**** error ****", strerror(errno));
    }
    RS_DBG0("recv ", bytesread);
    return bytesread;
}

void SocketRecvOperation::suspend()
{
    RS_DBG0("");
    socket->coroRecv_ = awaitingCoroutine_;
}
