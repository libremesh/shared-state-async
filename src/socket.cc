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

Socket::Socket(std::string_view port, IOContext& io_context)
    : AsyncFileDescriptor(io_context) 
    //: io_context_{io_context} //non static data_member initialization
{
    struct addrinfo hints, *res;

    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  //Use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; //TCP ... if you require UDP you must use SOCK_DGRAM
    hints.ai_flags = AI_PASSIVE;     //Fill in my IP for me

    getaddrinfo(NULL, port.data(), &hints, &res);
    fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    //todo: error ?
    int opt;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (bind(fd_, res->ai_addr, res->ai_addrlen) == -1)
        throw std::runtime_error{"bind"};
        //todo: fix
    listen(fd_, 8);
    //todo:error check
    fcntl(fd_, F_SETFL, O_NONBLOCK);
    io_context_.attach(this);
    io_context_.watchRead(this);
}

Socket::~Socket()
{
    std::cout << "------delete the socket(" << fd_ << ")\n";
    if (fd_ == -1)
    {
        std::cout << " socket(" << fd_ << ") already deleted \n";
        return;
    }
    io_context_.detach(this);
    close(fd_);
    fd_ = -1;
}

std::task<std::unique_ptr<Socket>> Socket::accept()
{
    //todo: deberia devolver unique 
    int fd = co_await SocketAcceptOperation{this};
    if (fd == -1)
        throw std::runtime_error{"accept"};
        //todo:
    std::cout << "aceptando";
    auto sharedsock = std::make_unique<Socket>(fd, io_context_);
    //std::cout << "+++ socket 1 "<< fd <<" use count "<< sharedsock.use_count()<< std::endl;
    co_return sharedsock;
}

SocketRecvOperation Socket::recv(void* buffer, std::size_t len)
{
    return SocketRecvOperation{this, buffer, len};
}
SocketSendOperation Socket::send(void* buffer, std::size_t len)
{
    return SocketSendOperation{this, buffer, len};
}

Socket::Socket(int fd, IOContext& io_context):AsyncFileDescriptor(fd,io_context)
{
    io_context_.attach(this);
}
