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

#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>
#include <system_error>

#include <util/rsnet.h>

#include "io_context.hh"
#include "connect_operation.hh"
#include "async_file_descriptor.hh"

#include <util/rsdebug.h>
#include <util/rsdebuglevel2.h>

ConnectOperation::ConnectOperation(
         AsyncFileDescriptor& socket, const sockaddr_storage& address,
        std::error_condition* ec ) :
    BlockSyscall<ConnectOperation, int>(ec),
    mSocket(socket), mAddr(address)
{
	RS_DBG2(socket, " ", sockaddr_storage_tostring(address));
	mSocket.getIOContext().watchWrite(&mSocket);
};


ConnectOperation::~ConnectOperation()
{
	RS_DBG4("");
	mSocket.getIOContext().unwatchWrite(&mSocket);
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

	RS_DBG4(mSocket);

	auto ret = connect(
	            mSocket.getFD(),
	            reinterpret_cast<const sockaddr*>(&mAddr),
	            len );

	if(ret && !errno)
	{
		/* On OpenWrt connect may fail returning -1 but forgetting to set
		 * errno to the error (so it remains 0), thus fooling the
		 * whole upstream error dealing logic, ultimately causing unexpected
		 * behaviours and crashes.
		 * Deal here with that situation overriding errno to avoid having to
		 * deal with it all over upstream code.
		 * Override errno with ERANGE as it shouldn't be normally raised by
		 * connect and so make it easy to detect this specific situation when
		 * propagated upstream. */
		RS_DBG1( mSocket, " connect to ", sockaddr_storage_tostring(mAddr),
		         " with ret: ", ret, " but forgetting to set errno: ", errno,
		         " overriding it with", std::errc::result_out_of_range );
		errno = ERANGE;
	}

	return ret;
}

void ConnectOperation::suspend()
{
	mSocket.addPendingOp(mAwaitingCoroutine);
}
