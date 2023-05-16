/*
 * Shared State
 *
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
#include <errno.h>


Socket::Socket(std::string_view port, IOContext& io_context)
    : AsyncFileDescriptor(io_context) 
{
    struct addrinfo hints, *res;

    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  //Use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; //TCP ... if you require UDP you must use SOCK_DGRAM
    hints.ai_flags = AI_PASSIVE;     //Fill in my IP for me

    getaddrinfo(NULL, port.data(), &hints, &res);
    fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    int opt;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (bind(fd_, res->ai_addr, res->ai_addrlen) == -1)
    {
        rs_error_bubble_or_exit(rs_errno_to_condition(errno),errorcontainer);
    }
    listen(fd_, 8);
    fcntl(fd_, F_SETFL, O_NONBLOCK);
    io_context_.attach(this);
    io_context_.watchRead(this);
}

Socket::~Socket()
{
    RS_DBG0("------delete the socket(" , fd_ , ")\n");
    if (fd_ == -1)
    {
        RS_WARN(" socket(" , fd_ , ") already deleted \n");
        return;
    }
    io_context_.detach(this);
    close(fd_);
    fd_ = -1;
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
