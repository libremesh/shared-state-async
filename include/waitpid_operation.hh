/*
 * Shared State
 *
 * Copyright (c) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include <sys/types.h>

#include "awaitable_syscall.hh"

class AsyncCommand;

class WaitpidOperation:
        public AwaitableSyscall<WaitpidOperation, pid_t, true>
{
public:
	WaitpidOperation(
	        AsyncCommand& afd,
	        int* wstatus = nullptr,
	        std::error_condition* ec = nullptr );
	~WaitpidOperation();

	pid_t syscall();

private:
	pid_t childPid() const;
	int *const mWstatus;
};
