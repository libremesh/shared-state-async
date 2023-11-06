/*
 * Shared State
 *
 * Copyright (c) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include <sys/socket.h>

#include "awaitable_syscall.hh"


class AsyncFileDescriptor;

/**
 * @brief Wrap connect system call for asynchronous operation
 */
class ConnectOperation : public AwaitableSyscall<ConnectOperation, int>
{
public:
	ConnectOperation(
	        AsyncFileDescriptor& socket, const sockaddr_storage& address,
	        std::error_condition* ec = nullptr );
	~ConnectOperation();

	int syscall();

private:
	sockaddr_storage mAddr;
	bool mFirstRun = true;
};
