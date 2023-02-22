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
#include "file_read_operation.hh"
#include <iostream>
#include "async_command.hh"
#include "async_file_desc.hh"
#include <unistd.h>



FileReadOperation::FileReadOperation(std::shared_ptr<AsyncFileDescriptor> socket,
        void* buffer,
        std::size_t len)
    : BlockSyscall{}
    , socket{socket}
    , buffer_{buffer}
    , len_{len}
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    socket->io_context_.watchRead(socket.get());
    std::cout << "FileReadOperation created\n";
}

FileReadOperation::~FileReadOperation()
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    socket->io_context_.unwatchRead(socket.get());
    std::cout << "~FileReadOperation\n";
}

ssize_t FileReadOperation::syscall()
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    std::cout << "FileReadOperation reading(" << socket->fd_ << (char *)buffer_<< len_<< "\n";
    ssize_t bytesread = read(socket->fd_, (char *)buffer_, len_);
    /* this method is invoked at least once but the pipe is not free. 
     * this is not problem since the BlockSyscall::await_suspend will test for -1 return value and test errno (EWOULDBLOCK or EAGAIN)
     * and then suspend the execution until a new notification arrives
    */
    if (bytesread == -1)
    {
         std::cout<< "**** error ****" << strerror(errno) << std::endl;
    }
    std::cout<<"Read "<< bytesread << " bytes" << std::endl;
    return bytesread;
}

void FileReadOperation::suspend()
{
        std::cout << __PRETTY_FUNCTION__ << " " <<  std::endl;

    socket->coroRecv_ = awaitingCoroutine_;
}
