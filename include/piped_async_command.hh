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
#include "async_file_desc.hh"
#include "io_context.hh"
#include "file_read_operation.hh"
#include "file_write_operation.hh"
#include "socket.hh"



/// @brief PipedAsyncCommand implementation using popen or pipe fork excec
class PipedAsyncCommand
{
public:
    /* Listen tcp non blocking socket */
    PipedAsyncCommand(const PipedAsyncCommand&) = delete;
    PipedAsyncCommand(std::string cmd, AsyncFileDescriptor* socket);
    PipedAsyncCommand(std::string cmd, IOContext& context);
    ~PipedAsyncCommand();

    FileReadOperation readpipe(void* buffer, std::size_t len);
    FileWriteOperation writepipe(void* buffer, std::size_t len);

private:
    friend FileReadOperation;
    friend AsyncFileDescriptor;
    friend Socket;
    // we need two file descriptors to interact with the forked process
    //      parent        child
    //      fd1[1]        fd1[0]
    //        4 -- fd_W --> 3 
    //      fd2[0]        fd2[1]
    //        5 <-- fd_r -- 6 
    int fd_w[2];
    int fd_r[2]; 
    pid_t forked_proces_id;
    std::shared_ptr<AsyncFileDescriptor> async_read_end_fd;
    std::shared_ptr<AsyncFileDescriptor> async_write_end_fd;

};

