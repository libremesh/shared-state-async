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
#include "popen_file_read_operation.hh"
#include "socket.hh"

/**
 * @brief AsyncCommand implementation using popen.
 * @warning this implementation is under development, you can use piped_async
 */
class PopenAsyncCommand :AsyncFileDescriptor
{
public:
    PopenAsyncCommand(const PopenAsyncCommand&) = delete;
    PopenAsyncCommand(PopenAsyncCommand&& command);
    PopenAsyncCommand(FILE * fdFromStream, AsyncFileDescriptor* socket);
    PopenAsyncCommand(std::string cmd, AsyncFileDescriptor* socket);
    ~PopenAsyncCommand();

    PopenFileReadOperation recvfile(uint8_t* buffer, std::size_t len);


private:
    friend PopenFileReadOperation;
    FILE * mPipe= nullptr;
    friend IOContext;
    explicit PopenAsyncCommand(int fd, IOContext& io_context);

};
