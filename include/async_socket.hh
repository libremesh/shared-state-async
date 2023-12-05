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

#include <memory>
#include <sys/socket.h>

#include "async_file_descriptor.hh"
#include "task.hh"

class IOContext;

/**
 * @brief Non blocking socket
 */
class AsyncSocket : public AsyncFileDescriptor
{
public:
	AsyncSocket(const AsyncSocket&) = delete;

	std::task<ssize_t> recv(
	        uint8_t *buffer, std::size_t len,
	        std::error_condition* errbub = nullptr );

	std::task<ssize_t> send(
	        const uint8_t* buffer, std::size_t len,
	        std::error_condition* errbub = nullptr );

	bool getPeerAddr(
	        sockaddr_storage& peerAddr,
	        std::error_condition* errbub = nullptr );

protected:
	friend IOContext;
	AsyncSocket(int fd, IOContext& io_context):
	    AsyncFileDescriptor(fd, io_context) {}
};

class ConnectingSocket: public AsyncSocket
{
public:
	ConnectingSocket() = delete;
	ConnectingSocket(const ConnectingSocket&) = delete;

	static std::task<std::shared_ptr<ConnectingSocket>> connect(
	        const sockaddr_storage& address,
	        IOContext& ioContext,
	        std::error_condition* errbub = nullptr );

protected:
	friend IOContext;
	ConnectingSocket(int fd, IOContext &io_context):
	    AsyncSocket(fd, io_context) {}
};

class ListeningSocket: public AsyncFileDescriptor
{
public:
	ListeningSocket() = delete;
	ListeningSocket(const ListeningSocket&) = delete;

	std::task<std::shared_ptr<AsyncSocket>> accept();

	static std::shared_ptr<ListeningSocket> setupListener(
	        uint16_t port, IOContext& ioContext,
	        std::error_condition* ec = nullptr );

protected:
	friend IOContext;
	ListeningSocket(int fd, IOContext& io_context):
	    AsyncFileDescriptor(fd, io_context) {}

	static constexpr int DEFAULT_LISTEN_BACKLOG = 8;
};
