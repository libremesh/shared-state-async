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
#include "socket_send_operation.hh"
#include <iostream>
#include "socket.hh"

SocketSendOperation::SocketSendOperation(Socket *socket,
                                         void *buffer,
                                         std::size_t len)
    : BlockSyscall{}, socket{socket}, buffer_{buffer}, len_{len}
{
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;

    socket->io_context_.watchWrite(socket);
    std::cout << "socket_send_operation\n";
}

SocketSendOperation::~SocketSendOperation()
{
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;

    socket->io_context_.unwatchWrite(socket);
    std::cout << "~socket_send_operation\n";
}

ssize_t SocketSendOperation::syscall()
{
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;

    std::cout << "send(" << socket->fd_ << "content" << (char *)buffer_ << "ammount" << len_ << ")\n";
    return send(socket->fd_, buffer_, len_, 0);
}

void SocketSendOperation::suspend()
{
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;

    socket->coroSend_ = awaitingCoroutine_;
}
