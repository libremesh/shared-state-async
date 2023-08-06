/*
 * Shared State
 *
 * Copyright (c) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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
#include "socket.hh"
#include "debug/rsdebug.h"

SocketRecvOperation::SocketRecvOperation(
        Socket& socket,
        uint8_t* buffer, std::size_t len,
        std::error_condition* ec ):
    BlockSyscall{ec}, mSocket{socket}, mBuffer{buffer}, mLen{len}
{
	mSocket.io_context_.watchRead(&mSocket);
}

SocketRecvOperation::~SocketRecvOperation()
{
	mSocket.io_context_.unwatchRead(&mSocket);
}

ssize_t SocketRecvOperation::syscall()
{
	ssize_t bytesread = recv(mSocket.mFD, mBuffer, mLen, 0);

	/* this method is invoked at least once but the socket is not free.
	 * this is not problem since the BlockSyscall::await_suspend will test for
	 * -1 return value and test errno (EWOULDBLOCK or EAGAIN)
	 * and then suspend the execution until a new notification arrives
	 */
	if (bytesread == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno),
		            mError, "recv failed" );
	}
	else
	{
		mBuffer[bytesread]='\0';
		mBuffer[bytesread+1]='\0';
		bytesread = bytesread +2;
	}

	return bytesread;
}

void SocketRecvOperation::suspend()
{
	mSocket.coroRecv_ = mAwaitingCoroutine;
}
