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
#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"
#include "file_read_operation.hh"
#include "task.hh"
#include "debug/rsdebug.h"

/**
 * @brief Listen tcp non blocking socket
 * 
 */
class Socket : public AsyncFileDescriptor
{
public:
    Socket(std::string_view port, IOContext &io_context);
    Socket(const Socket &) = delete;
    Socket(Socket &&socket);

    ~Socket();

    std::task<std::unique_ptr<Socket>> accept();

    SocketRecvOperation recv(uint8_t *buffer, std::size_t len,std::shared_ptr<std::error_condition> ec=nullptr);
    SocketSendOperation send(uint8_t *buffer, std::size_t len);
    explicit Socket(int fd, IOContext &io_context);

private:
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    friend FileReadOperation;
    FILE *pipe = nullptr;
    friend IOContext;
    std::error_condition *errorcontainer = nullptr;
};
