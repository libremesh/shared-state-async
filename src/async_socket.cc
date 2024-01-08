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

#include "async_socket.hh"
#include "connect_operation.hh"
#include "accept_operation.hh"
#include "recv_operation.hh"
#include "send_operation.hh"
#include "io_context.hh"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <sys/time.h>

#include <util/rsnet.h>
#include <util/rserrorbubbleorexit.h>
#include <util/stacktrace.h>
#include <util/rsdebug.h>
#include <util/rsdebuglevel1.h>

std::task<std::shared_ptr<ConnectingSocket>> ConnectingSocket::connect(
        const sockaddr_storage& address,
        IOContext& ioContext, std::error_condition* errbub )
{
	int fd = socket(PF_INET6, SOCK_STREAM, 0);
	if(fd < 0)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub, "creating socket" );
		co_return nullptr;
	}

	auto lSocket = ioContext.registerFD<ConnectingSocket>(fd);
	ioContext.attachWriteOnly(lSocket.get());

	if(co_await ConnectOperation(*lSocket, address, errbub) == -1)
	{
		co_await ioContext.closeAFD(lSocket);
		co_return nullptr;
	}

	co_return lSocket;
}

std::shared_ptr<ListeningSocket> ListeningSocket::setupListener(
        uint16_t port, IOContext& ioContext, std::error_condition* ec )
{
	int fd_ = socket(PF_INET6, SOCK_STREAM, 0);
	if(fd_ < 0)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), ec, "creating socket" );
		return nullptr;
	}

#ifdef IPV6_V6ONLY
	int ipv6only_optval = 0;
	if( setsockopt( fd_, IPPROTO_IPV6, IPV6_V6ONLY,
	                &ipv6only_optval, sizeof(ipv6only_optval) ) < 0 )
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), ec,
		            "setting IPv6 socket dual stack" );
		return nullptr;
	}
#endif // IPV6_V6ONLY

	int reuseaddr_optval = 1;
	if( setsockopt( fd_, SOL_SOCKET, SO_REUSEADDR,
	                &reuseaddr_optval, sizeof(reuseaddr_optval) ) < 0 )
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), ec, "setting SO_REUSEADDR" );
		return nullptr;
	}

	sockaddr_in6 listenAddr;
	memset(&listenAddr, 0, sizeof(listenAddr));
	listenAddr.sin6_family = AF_INET6;
	listenAddr.sin6_port = htons(port);

	if( bind( fd_, reinterpret_cast<const struct sockaddr *>(&listenAddr),
	          sizeof(listenAddr) ) < 0 )
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), ec, "bind" );
		return nullptr;
	}

	if( listen(fd_, DEFAULT_LISTEN_BACKLOG) < 0 )
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), ec, "listen" );
		return nullptr;
	}

	auto lSocket = ioContext.registerFD<ListeningSocket>(fd_, ec);
	if(!lSocket)
	{
		if(close(fd_))
		{
			rs_error_bubble_or_exit(
			            rs_errno_to_condition(errno), ec,
			            " failure closing socket: ", fd_,
			            " after failed registerFD. Very weird!" );
		}
		return nullptr;
	}
	ioContext.attachReadonly(lSocket.get());
	ioContext.watchRead(lSocket.get());
	return lSocket;
}

std::task<std::shared_ptr<AsyncSocket>> ListeningSocket::accept()
{
	int fd = co_await AcceptOperation(*this);
	auto rsk = mIOContext.registerFD<AsyncSocket>(fd);
	mIOContext.attach(rsk.get());
	co_return rsk;
}

std::task<ssize_t> AsyncSocket::recv(
        uint8_t* buffer, std::size_t len,
        std::error_condition* errbub )
{
	RS_DBG2( *this,
	         " buffer: ", reinterpret_cast<const void*>(buffer),
	         " len: ", len, " errbub: ", errbub );

	ssize_t numReadBytes = 0;
	ssize_t totalReadBytes = 0;
	do
	{
		numReadBytes = co_await
		        RecvOperation(
		            *this, buffer + totalReadBytes, len - totalReadBytes, errbub );
		if(numReadBytes == -1) RS_UNLIKELY co_return -1;

		totalReadBytes += numReadBytes;
	}
	while(numReadBytes && totalReadBytes < len);

	co_return totalReadBytes;
}

std::task<ssize_t> AsyncSocket::send(
        const uint8_t* buffer, std::size_t len,
        std::error_condition* errbub )
{
	RS_DBG2( *this,
	         " buffer: ", reinterpret_cast<const void*>(buffer),
	         " len: ", len, " errbub: ", errbub );
	RS_DBG4( " buffer content: ",
	         std::string(reinterpret_cast<const char*>(buffer), len) );

	ssize_t numWriteBytes = 0;
	ssize_t totalWriteBytes = 0;
	do
	{
		numWriteBytes = co_await
		        SendOperation(
		            *this, buffer + totalWriteBytes, len - totalWriteBytes, errbub );

		if(numWriteBytes == -1) RS_UNLIKELY
		    co_return -1;

		totalWriteBytes += numWriteBytes;
	}
	while(numWriteBytes && totalWriteBytes < len);

	co_return totalWriteBytes;
}

bool AsyncSocket::getPeerAddr(
        sockaddr_storage& peerAddr,
        std::error_condition* errbub )
{
	socklen_t peerLen = sizeof(peerAddr);
	int getPeerNameRet =
	        getpeername(mFD, reinterpret_cast<sockaddr*>(&peerAddr), &peerLen );

	if(getPeerNameRet)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "getpeername failed");
		return false;
	}

	return true;
}
