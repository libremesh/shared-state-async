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

#include "close_operation.hh"
#include "async_file_descriptor.hh"

#include <unistd.h>

int CloseOperation::syscall()
{
	/* IMHO the close(2) manual is not explaining enough close() behaviour on
	 * non-blocking file descriptors, but it insists on not calling close() more
	 * then once on the same FD.
	 *
	 * From @see man 2 close
	 * Retrying the close() after a failure return is the wrong thing to do,
	 * since this may cause a reused file descriptor from another thread to be
	 * closed.  This can occur because the Linux kernel always releases the file
	 * descriptor early in the close operation, freeing it for reuse; the steps
	 * that may return an  error,  such as flushing data to the filesystem or
	 * device, occur only later in the close operation.
	 *
	 * This GNU coreutils command patch seems to confirm that close should not
	 * be called again even on EAGAIN
	 * @see https://lists.gnu.org/archive/html/coreutils/2018-09/msg00010.html
	 *
	 * Also the discussion on this python bug suggests the same
	 * @see https://bugs.python.org/issue25476
	 */

	auto sysCloseRet = close(mAFD.getFD());
	if(shouldWait(sysCloseRet, errno))
	{
		/* If the system inform us that some stuff may be still pending on the
		 * FD kernel side just lie to AwaitableSyscall to enforce not suspending
		 * and so not being called again. User space side the FD is closed NOW
		 * anyway even if FD is non-blocking. */

		RS_DBG("Stepped into delayed close ", rs_errno_to_condition(errno));

		errno = 0;
		return 0;
	}

	if(sysCloseRet)
	{
		RS_ERR( "close syscall failed badly ", sysCloseRet, " ",
			   rs_errno_to_condition(errno) );
		print_stacktrace();
	}

	/* Success without delay or other kind of errors just follow the
	 * conventional AwaitableSyscall path */
	return sysCloseRet;
}
