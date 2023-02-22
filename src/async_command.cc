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
#include "async_command.hh"
#include <iostream>

AsyncCommand::AsyncCommand(FILE *fdFromStream, AsyncFileDescriptor *socket)
    : AsyncFileDescriptor(socket->io_context_), pipe{fdFromStream}
{
    fd_ = fileno(fdFromStream);

    int flags = fcntl(fd_, F_GETFL, 0);
    // put into "nonblocking mode"
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    io_context_.attachReadonly(this);
    // io_context_.watchRead(this);
    RS_DBG0("")<< "AsyncCommand created and filedescriptor # " << fd_ << std::endl;
}

AsyncCommand::AsyncCommand(std::string cmd, AsyncFileDescriptor *socket): AsyncFileDescriptor(socket->io_context_)
{
    pipe = popen(cmd.c_str(), "r");
    // partir el popen
    if (!pipe)
    {
        std::cout<<"we have a problem...";
    }
        
    fd_ = fileno(pipe);

    int flags = fcntl(fd_, F_GETFL, 0);
    // put into "nonblocking mode"
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    io_context_.attachReadonly(this);
    // io_context_.watchRead(this);
    RS_DBG0("")<< "AsyncCommand created and filedescriptor # " << fd_ << std::endl;
}



AsyncCommand::~AsyncCommand()
{
    RS_DBG0("")<< "------ delete the AsyncCommand(" << fd_ << ")\n";
    if (fd_ == -1)
        return;
    io_context_.detach(this);
    close(fd_);
    pclose(pipe);
    
}

PipeFileReadOperation AsyncCommand::recvfile(void *buffer, std::size_t len)
{
    return PipeFileReadOperation{this, buffer, len};
}
