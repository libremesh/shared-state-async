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
#include "async_socket.hh"
#include "async_file_descriptor.hh"

#include <util/rsdebug.h>
#include <util/rsdebuglevel0.h>

ConnectOperation::ConnectOperation(
         ConnectingSocket& pSocket, const sockaddr_storage& address,
        std::error_condition* ec ) :
    AwaitableSyscall<ConnectOperation, int>(pSocket, ec), mAddr(address)
{
	RS_DBG2(socket, " ", sockaddr_storage_tostring(address));
	pSocket.getIOContext().watchWrite(&pSocket);
};


ConnectOperation::~ConnectOperation()
{
	RS_DBG4("");
	mAFD.getIOContext().unwatchWrite(&mAFD);
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
		            mAFD.getFD(),
		            reinterpret_cast<const sockaddr*>(&mAddr),
		            sizeof(struct sockaddr_in6) );
	}

	sockaddr_storage mPeerAddr;
	socklen_t peerLen = sizeof(mPeerAddr);
	int getPeerNameRet =
	        getpeername(
	            mAFD.getFD(),
	            reinterpret_cast<sockaddr*>(&mPeerAddr), &peerLen );

	if(getPeerNameRet)
	{
		RS_DBG1( "Failure connecting to: ", sockaddr_storage_tostring(mAddr),
		         " on: ", mAFD );

		if(errno == ENOTCONN)
		{
			char mDiscard;
			int readRet = read(mAFD.getFD(), &mDiscard, 1);
			int readErr = errno;
			RS_DBG1( "Failure connecting to: ", sockaddr_storage_tostring(mAddr),
			         " on: ", mAFD, " with: ", rs_errno_to_condition(readErr) );
			return readRet;
		}

		return -1;
	}

	RS_DBG1( "Successful connection to: ", sockaddr_storage_tostring(mPeerAddr),
	         " on: ", mAFD );
	return 0;
}
