/*
 * Shared State
 *
 * Copyright (C) 2024  Gioacchino Mazzurco <gio@eigenlab.org>
 * Copyright (C) 2024  Asociación Civil Altermundi <info@altermundi.net>
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

#include <sys/timerfd.h>

#include "async_timer.hh"
#include "io_context.hh"
#include "read_operation.hh"

#include <util/rsdebug.h>
#include <util/rsdebuglevel1.h>

/*static*/ std::shared_ptr<AsyncTimer> AsyncTimer::create(
        IOContext& ioContext,
        std::error_condition* errbub )
{
	int timerFD = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);

	auto timerAFD = ioContext.registerFD<AsyncTimer>(timerFD, errbub);
	ioContext.attachReadonly(timerAFD.get());

	return timerAFD;
}

std::task<bool> AsyncTimer::wait(
        std::chrono::seconds wsec, std::chrono::nanoseconds wnsec,
        std::error_condition* errbub )
{
	if(wsec < std::chrono::seconds::zero()) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
		            std::errc::invalid_argument, errbub,
		            "negative wait seconds" );
		co_return false;
	}

	if(wnsec < std::chrono::nanoseconds::zero()) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
		            std::errc::invalid_argument, errbub,
		            "negative wait nanoseconds" );
		co_return false;
	}

	if(wnsec > MAX_WAIT_NANOSECONDS) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
		            std::errc::invalid_argument, errbub,
		            "wait nanoseconds > ", MAX_WAIT_NANOSECONDS );
		co_return false;
	}

	struct itimerspec tTimeSpec{};
	tTimeSpec.it_interval.tv_sec = 0;
	tTimeSpec.it_interval.tv_nsec = 0;
	tTimeSpec.it_value.tv_sec = wsec.count();
	tTimeSpec.it_value.tv_nsec = wnsec.count();

	if(-1 == timerfd_settime(getFD(), 0, &tTimeSpec, nullptr)) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "timerfd_settime failed" );
		co_return false;
	}

	uint64_t tBuff = -1;
	ssize_t numReadBytes = co_await ReadOp {
	            *this,
	            reinterpret_cast<uint8_t*>(&tBuff), sizeof(tBuff),
	            errbub };

	co_return numReadBytes == sizeof(tBuff);
}
