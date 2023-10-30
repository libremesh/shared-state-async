/*
 * Shared State
 *
 * Copyright (C) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include <sys/socket.h>

#include "socket_send_operation.hh"
#include "socket.hh"

SocketSendOperation::SocketSendOperation(
        Socket& socket, const uint8_t* buffer, std::size_t len,
        std::error_condition* ec ):
    BlockSyscall{ec}, mSocket{socket}, mBuffer{buffer}, mLen{len}
{
	mSocket.getIOContext().watchWrite(&mSocket);
}

SocketSendOperation::~SocketSendOperation()
{
	mSocket.getIOContext().unwatchWrite(&mSocket);
}

ssize_t SocketSendOperation::syscall()
{
	return send(mSocket.getFD(), mBuffer, mLen, 0);
}

void SocketSendOperation::suspend()
{
	mSocket.addPendingOp(mAwaitingCoroutine);
}
