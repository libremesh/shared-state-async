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

#include "socket.hh"
#include "debug/rsdebuglevel2.h"
#include "connect_operation.hh"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>

std::task<std::unique_ptr<ConnectingSocket>> ConnectingSocket::connect(
        const sockaddr_storage& address,
        IOContext& ioContext, std::error_condition* ec )
{
	int fd_ = socket(PF_INET6, SOCK_STREAM, 0);
	if(fd_ < 0)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), ec, "creating socket" );
		co_return nullptr;
	}

	if( fcntl(fd_, F_SETFL, O_NONBLOCK) < 0 )
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), ec, "O_NONBLOCK" );
		co_return nullptr;
	}

	auto lSocket = std::unique_ptr<ConnectingSocket>(
	            new ConnectingSocket(fd_, ioContext) );
	ioContext.watchRead(lSocket.get());

	co_await ConnectOperation(*lSocket.get(), address, ec);

	co_return lSocket;
}

std::unique_ptr<ListeningSocket> ListeningSocket::setupListener(
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

	if( fcntl(fd_, F_SETFL, O_NONBLOCK) < 0 )
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), ec, "O_NONBLOCK" );
		return nullptr;
	}

	auto lSocket = std::unique_ptr<ListeningSocket>(
	            new ListeningSocket(fd_, ioContext) );
	ioContext.watchRead(lSocket.get());
	return lSocket;
}

std::task<std::unique_ptr<Socket>> ListeningSocket::accept()
{
	int fd = co_await SocketAcceptOperation(*this);
	co_return std::unique_ptr<Socket>(new Socket(fd, io_context_));
}

SocketRecvOperation Socket::recv(
        uint8_t* buffer, std::size_t len,
        std::error_condition* ec )
{
	return SocketRecvOperation(*this, buffer, len, ec);
}

SocketSendOperation Socket::send(
        const uint8_t* buffer, std::size_t len,
        std::error_condition* ec )
{
	return SocketSendOperation{*this, buffer, len, ec};
}

Socket::Socket(int fd, IOContext& io_context):
    AsyncFileDescriptor(fd,io_context)
{
	io_context_.attach(this);
}
