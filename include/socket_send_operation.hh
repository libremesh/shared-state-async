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

#include <sys/socket.h>
#include <sys/types.h>

#include "block_syscall.hh"

class Socket;
/**
 * @brief Implements an asynchronous Socket Send Operation
 * 
 */
class SocketSendOperation : public BlockSyscall<SocketSendOperation, ssize_t>
{
public:
    SocketSendOperation(Socket *socket, const uint8_t *buffer, std::size_t len, std::shared_ptr<std::error_condition> ec = nullptr);
    ~SocketSendOperation();

    ssize_t syscall();
    void suspend();

private:
    Socket *socket;
    const uint8_t *buffer_;
    std::size_t len_;
};
