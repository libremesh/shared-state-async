/*
 * Shared State
 *
 * Copyright (C) 2023-2024  Gioacchino Mazzurco <gio@eigenlab.org>
 * Copyright (c) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (c) 2023  Instituto Nacional de Tecnología Industrial
 * Copyright (C) 2023-2024  Asociación Civil Altermundi <info@altermundi.net>
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

#include "write_operation.hh"
#include "async_file_descriptor.hh"
#include "io_context.hh"

#include <unistd.h>


WriteOp::WriteOp(
        AsyncFileDescriptor& AFD,
        const uint8_t* buffer, std::size_t len,
        std::error_condition* ec ):
    AwaitableSyscall{AFD, ec}, mBuffer{buffer}, mLen{len}
{
	mAFD.getIOContext().watchWrite(&mAFD);
}

WriteOp::~WriteOp()
{
	mAFD.getIOContext().unwatchWrite(&mAFD);
}

ssize_t WriteOp::syscall()
{
	ssize_t bytes_writen = write(
	            mAFD.getFD(),
	            reinterpret_cast<const char*>(mBuffer), mLen );

	return bytes_writen;
}
