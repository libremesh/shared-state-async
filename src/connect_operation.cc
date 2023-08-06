/*
 * Shared State
 *
 * Copyright (c) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include "connect_operation.hh"
#include "debug/rsdebuglevel4.h"

#include <sys/socket.h>
#include <netinet/in.h>

ConnectOperation::ConnectOperation(
         AsyncFileDescriptor& socket, const sockaddr_storage& address,
        std::error_condition* ec ) :
    BlockSyscall<ConnectOperation, int>(ec),
    mSocket(socket), mAddr(address) {};


ConnectOperation::~ConnectOperation()
{
	mSocket.io_context_.unwatchRead(&mSocket);
}

int ConnectOperation::syscall()
{
	socklen_t len = 0;
	switch (mAddr.ss_family)
	{
	case AF_INET:
		len = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		len = sizeof(struct sockaddr_in6);
		break;
	}

	RS_DBG2( "Connecting to fd: ",  mSocket.mFD );

	return connect(
	            mSocket.mFD,
	            reinterpret_cast<const sockaddr*>(&mAddr),
	            len );
}

void ConnectOperation::suspend()
{
	/* TODO: connect is neither read or write per se ATM it seems to work like
	 * this, but we should think a bit more about this */
	mSocket.coroRecv_ = mAwaitingCoroutine;
}
