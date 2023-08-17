/*
 * Shared State
 *
 * Copyright (C) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
 * Copyright (C) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (C) 2023  Instituto Nacional de Tecnología Industrial
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
#include "socket.hh"

SocketAcceptOperation::SocketAcceptOperation(
        ListeningSocket& socket, std::error_condition* ec ):
    BlockSyscall(ec), mLSocket(socket)
{
	RS_DBG3("");
	mLSocket.io_context_.watchRead(&mLSocket);
}

SocketAcceptOperation::~SocketAcceptOperation()
{
	RS_DBG3("");
	mLSocket.io_context_.unwatchRead(&mLSocket);
}

int SocketAcceptOperation::syscall()
{
	sockaddr_storage their_addr;
	socklen_t addr_size = sizeof their_addr;

	/* TODO: Saving/recording the address of the peer connecting to us might be
	 * useful for debugging */

	return accept(mLSocket.mFD, (struct sockaddr *)&their_addr, &addr_size);
}

void SocketAcceptOperation::suspend()
{
	RS_DBG3("");
	mLSocket.coroRecv_ = mAwaitingCoroutine;
}
