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

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include "io_context.hh"
#include "block_syscall.hh"
#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"
#include "file_read_operation.hh"
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

 static int mTotalAsyncFileDescriptor;

class AsyncFileDescriptor
{
public:
    /* Listen tcp non blocking socket */
    AsyncFileDescriptor(IOContext &io_context) : io_context_{io_context}
    {
    }
    AsyncFileDescriptor(const AsyncFileDescriptor &) = delete;
    AsyncFileDescriptor(AsyncFileDescriptor &&socket)
        : io_context_{socket.io_context_}, fd_{socket.fd_}, io_state_{socket.io_state_}, io_new_state_{socket.io_new_state_}
    {
        socket.fd_ = -1;
        mTotalAsyncFileDescriptor = (mTotalAsyncFileDescriptor + 1) % 50;
        number = mTotalAsyncFileDescriptor;
    }

    AsyncFileDescriptor(int fd, IOContext &io_context)
        : io_context_{io_context}, fd_{fd}
    {
        mTotalAsyncFileDescriptor = (mTotalAsyncFileDescriptor + 1) %50;
        number = mTotalAsyncFileDescriptor;
        RS_DBG0("AsyncFileDescriptor ", fd, "Created ", "AsyncFileDescriptor ", number);
        fcntl(fd_, F_SETFL, O_NONBLOCK);
        // io_context_.attach(this);
    }

    ~AsyncFileDescriptor()
    {
        RS_DBG0("------delete the AsyncFileDescriptor(", fd_, ")"," AsyncFileDescriptor ", number);
        number = -1;
        if (fd_ == -1)
        {
            return;
        }
        io_context_.detach(this);
        close(fd_);
        fd_ = -1;
    }

    bool resumeRecv()
    {
        RS_DBG0("resumeRecv AsyncFileDescriptor ", number);
        coroRecv_.resume();
        return true;
    }

    bool resumeSend()
    {
        RS_DBG0("resumeSend AsyncFileDescriptor ", number);
        coroSend_.resume();
        return true;
    }

    // protected:
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    friend FileReadOperation;
    friend IOContext;
    IOContext &io_context_;
    int fd_ = -1;
    uint32_t io_state_ = 0;
    uint32_t io_new_state_ = 0;
    int number = 0;
    std::coroutine_handle<> coroRecv_;
    std::coroutine_handle<> coroSend_;
};
