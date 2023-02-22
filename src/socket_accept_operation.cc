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
#include "socket_accept_operation.hh"
#include <iostream>
#include "socket.hh"

SocketAcceptOperation::SocketAcceptOperation(Socket *socket)
    : BlockSyscall{}, socket{socket}
{
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;

    socket->io_context_.watchRead(socket);
    std::cout << "socket_accept_operation\n";
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;
}

SocketAcceptOperation::~SocketAcceptOperation()
{
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;

    socket->io_context_.unwatchRead(socket);
    std::cout << "~socket_accept_operation\n";
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;
}

int SocketAcceptOperation::syscall()
{
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;

    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof their_addr;
    std::cout << "accept(" << socket->fd_ << ", ...)" << std::endl;
    return accept(socket->fd_, (struct sockaddr *)&their_addr, &addr_size);
}

void SocketAcceptOperation::suspend()
{
    std::cout << __PRETTY_FUNCTION__ << " " << std::endl;

    socket->coroRecv_ = awaitingCoroutine_;
}
