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
	/* Detecting errors on non-blocking connect is not trivial
	 * @see https://stackoverflow.com/questions/17769964/linux-sockets-non-blocking-connect
	 */

	if(mFirstRun)
	{
		mFirstRun = false;

		if( !sockaddr_storage_isValidNet(mAddr) ||
		        !sockaddr_storage_ipv4_to_ipv6(mAddr) )
		{
			RS_ERR("Invalid address: ", sockaddr_storage_tostring(mAddr));
			print_stacktrace();
			errno = EINVAL;
			return -1;
		}

		return connect(
		            mSocket.getFD(),
		            reinterpret_cast<const sockaddr*>(&mAddr),
		            sizeof(struct sockaddr_in6) );
	}

	socklen_t peerLen = 0;
	sockaddr_storage mPeerAddr;
	int getPeerNameRet =
	        getpeername(
	            mSocket.getFD(),
	            reinterpret_cast<sockaddr*>(&mPeerAddr), &peerLen );

	if(getPeerNameRet)
	{
		RS_DBG1( "Failure connecting to: ", sockaddr_storage_tostring(mAddr),
		         " on: ", mSocket );

		if(errno == ENOTCONN)
		{
			char mDiscard;
			int readRet = read(mSocket.getFD(), &mDiscard, 1);
			int readErr = errno;
			RS_DBG1( "Failure connecting to: ", sockaddr_storage_tostring(mAddr),
			         " on: ", mSocket, " with: ", rs_errno_to_condition(readErr) );
			return readRet;
		}

		return -1;
	}

	RS_DBG1( "Successful connection to: ", sockaddr_storage_tostring(mPeerAddr),
	         " on: ", mSocket );
	return 0;
}

void ConnectOperation::suspend()
{
	mSocket.addPendingOp(mAwaitingCoroutine);
}
