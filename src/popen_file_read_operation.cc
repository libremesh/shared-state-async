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

#include "popen_file_read_operation.hh"
#include "popen_async_command.hh"

#include <cstdio>

PopenFileReadOperation::PopenFileReadOperation(
        PopenAsyncCommand& cmd,
        uint8_t* buffer, std::size_t len,
        std::error_condition* ec ):
    BlockSyscall{ec},
    mCmd{cmd}, mBuffer{buffer}, mLen{len}
{
	mCmd.io_context_.watchRead(&mCmd);
}

PopenFileReadOperation::~PopenFileReadOperation()
{
	mCmd.io_context_.unwatchRead(&mCmd);
}

ssize_t PopenFileReadOperation::syscall()
{
	std::string result;

	while (!feof(mCmd.mPipe))
	{
		if(fgets(reinterpret_cast<char*>(mBuffer), mLen, mCmd.mPipe))
			result += reinterpret_cast<char*>(mBuffer); // TODO: WTF?!?!
	}

	// TODO: WTF?!?!
	return 10;
}

void PopenFileReadOperation::suspend()
{
	mCmd.coroRecv_ = mAwaitingCoroutine;
}
