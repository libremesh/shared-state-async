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
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include "debug/rsdebuglevel2.h"
#include <cerrno>

Socket::Socket(uint16_t port, IOContext& io_context)
    : AsyncFileDescriptor(io_context) 
{
	// TODO: DEAL WITH ERRORS!!!

	fd_ = socket(PF_INET6, SOCK_STREAM, 0);

	int err = 0;

#ifdef IPV6_V6ONLY
	int ipv6only_optval = 0;
	err = setsockopt( fd_, IPPROTO_IPV6, IPV6_V6ONLY,
	                  &ipv6only_optval, sizeof(ipv6only_optval) );
	RS_ERR("Failure setting IPv6 socket dual stack err: ", err);
#endif // IPV6_V6ONLY

	int reuseaddr_optval = 1;
	err = setsockopt( fd_, SOL_SOCKET, SO_REUSEADDR,
	                  &reuseaddr_optval, sizeof(reuseaddr_optval) );

	sockaddr_in6 listenAddr;
	memset(&listenAddr, 0, sizeof(listenAddr));
	listenAddr.sin6_port = port;

	bind( fd_, reinterpret_cast<const struct sockaddr *>(&listenAddr),
	      sizeof(listenAddr) );

	listen(fd_, 8);
	fcntl(fd_, F_SETFL, O_NONBLOCK);

	io_context_.attach(this);
	io_context_.watchRead(this);
}

Socket::~Socket()
{
    RS_DBG0("------delete the socket(" , fd_ , ")\n");
    /*if (fd_ == -1)
    {
        RS_WARN(" socket(" , fd_ , ") already deleted \n");
        return;
    }
    io_context_.detach(this);
    close(fd_);
    fd_ = -1;*/
}

std::task<std::unique_ptr<Socket>> Socket::accept()
{
    
    int fd = co_await SocketAcceptOperation{this}; //in case of failure the app will exit
    /*
    std::shared_ptr error_info = std::make_shared<std::error_condition>();
    //in case of failure, error_info wil have information about the problem.
    int fd = co_await SocketAcceptOperation{this,error_info}; 
    if (*error_info.get())
    {
        rs_error_bubble_or_exit(rs_errno_to_condition(errno),errorcontainer);
    }*/
    RS_DBG0("aceptando");
    auto clientsocket = std::make_unique<Socket>(fd, io_context_);
    co_return clientsocket;
}

SocketRecvOperation Socket::recv(uint8_t* buffer, std::size_t len,std::shared_ptr<std::error_condition> ec)
{
    return SocketRecvOperation{this, buffer, len,ec};
}
SocketSendOperation Socket::send(uint8_t* buffer, std::size_t len)
{
    return SocketSendOperation{this, buffer, len};
}

Socket::Socket(int fd, IOContext& io_context):AsyncFileDescriptor(fd,io_context)
{
    io_context_.attach(this);
}
