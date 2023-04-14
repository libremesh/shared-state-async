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
#pragma once

#include <set>
#include <stdexcept>
#include <sys/epoll.h>

#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"
#include "file_read_operation.hh"
#include "file_write_operation.hh"
#include "dying_process_wait_operation.hh"
#include "popen_file_read_operation.hh"

class Socket;
class AsyncFileDescriptor;
class PopenAsyncCommand;
class PipedAsyncCommand;

/**
 * This class works work as dispacher, notifying suspended blocksyscall when 
 * ready to be activated. blocksyscalls must suscribe to this class in order 
 * to be notified.
 * 
 * @brief This class is an Just an epoll wrapper 
 */
class IOContext
{
public:
    IOContext()
        : fd_{epoll_create1(0)}
    {
        if (fd_ == -1)
            throw std::runtime_error{"epoll_create1"};
            //todo: fix a esto no usaremos exept hacer un factory con un unique pointer que tire error
            //ver que pasa cuando falla 
    }

    void run();
private:
    constexpr static std::size_t max_events = 20;
    const int fd_; //iocontext epool fd

    // Fill it by watchRead / watchWrite
    std::set<AsyncFileDescriptor*> processedSockets;

    friend AsyncFileDescriptor;
    friend Socket;
    friend PopenAsyncCommand;
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    friend FileReadOperation;
    friend FileWriteOperation;
    friend PopenFileReadOperation;
    friend PipedAsyncCommand;
    friend DyingProcessWaitOperation;
    void attach(AsyncFileDescriptor* socket);
    void attachReadonly(AsyncFileDescriptor* socket);
    void attachWriteOnly(AsyncFileDescriptor* socket);
    void watchRead(AsyncFileDescriptor* socket);
    void unwatchRead(AsyncFileDescriptor* socket);
    void watchWrite(AsyncFileDescriptor* socket);
    void unwatchWrite(AsyncFileDescriptor* socket);
    void detach(AsyncFileDescriptor* socket);
};
