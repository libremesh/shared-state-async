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

#include <cstring>
#include <memory>

#include "async_file_desc.hh"
#include "io_context.hh"
#include "socket_accept_operation.hh"
#include "socket_recv_operation.hh"
#include "socket_send_operation.hh"
#include "file_read_operation.hh"
#include "task.hh"


/**
 * @brief Non blocking socket
 * TODO: Is this class needed or is AsyncFileDescriptor enough?
 */
class Socket : public AsyncFileDescriptor
{
public:
	Socket(const Socket&) = delete;

	SocketRecvOperation recv(
	        uint8_t *buffer, std::size_t len,
	        std::error_condition* ec = nullptr );

	SocketSendOperation send(
	        const uint8_t* buffer, std::size_t len,
	        std::error_condition* ec = nullptr );

protected:
	Socket(int fd, IOContext &io_context);

	friend SocketAcceptOperation;
	friend SocketRecvOperation;
	friend SocketSendOperation;
	friend ReadOp;
	friend IOContext;
	friend ListeningSocket;
};

class ConnectingSocket: public Socket
{
public:
	ConnectingSocket() = delete;
	ConnectingSocket(const ConnectingSocket&) = delete;

	static std::task<std::unique_ptr<ConnectingSocket>> connect(
	        const sockaddr_storage& address,
	        IOContext& ioContext,
	        std::error_condition* ec = nullptr );

protected:
	ConnectingSocket(int fd, IOContext &io_context): Socket(fd, io_context) {}
};

class ListeningSocket: public Socket
{
public:
	ListeningSocket() = delete;
	ListeningSocket(const ListeningSocket&) = delete;

	std::task<std::unique_ptr<Socket>> accept();

	static std::unique_ptr<ListeningSocket> setupListener(
	        uint16_t port, IOContext& ioContext,
	        std::error_condition* ec = nullptr );

protected:
	ListeningSocket(int fd, IOContext &io_context): Socket(fd, io_context)
	{
		io_context_.attach(this);
	}
	static constexpr int DEFAULT_LISTEN_BACKLOG = 8;
};
