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
#pragma once

#include <chrono>
#include <memory>

#include "async_file_descriptor.hh"
#include "io_context.hh"

#include <util/rsdebug.h>
#include <util/rsdebuglevel1.h>

/**
 * @brief Async timer
 */
class AsyncTimer : public AsyncFileDescriptor
{
public:
	/**
	 * Create an async timer
	 */
	static std::shared_ptr<AsyncTimer> create(
	        IOContext& ioContext,
	        std::error_condition* errbub = nullptr );

	/** Keep behaviour consistent with nanosleep altought we don't use it in our
	 *  async implementation @see `man nanosleep` */
	static auto constexpr MAX_WAIT_NANOSECONDS =
	        std::chrono::nanoseconds(999999999);

	/**
	 * @brief Asynchronously waits for timer expiration.
	 * Like nanosleep but co_await-able
	 * @return false on error true otherwise
	 */
	std::task<bool> wait(
	        std::chrono::seconds wsec,
	        std::chrono::nanoseconds wnsec = std::chrono::nanoseconds::zero(),
	        std::error_condition* errbub = nullptr );

	AsyncTimer(const AsyncTimer &) = delete;
	AsyncTimer() = delete;
	~AsyncTimer() = default;

protected:
	friend IOContext;
	AsyncTimer(int fd, IOContext &ioContext):
	    AsyncFileDescriptor(fd, ioContext) {}
};
