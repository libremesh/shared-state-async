/*
 * Shared State
 *
 * Copyright (c) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (c) 2023  Instituto Nacional de Tecnología Industrial
 * Copyright (C) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include "io_context.hh"
#include "socket.hh"
#include "task.hh"
#include "sharedstate.hh"
#include <iostream>
#include <array>
#include <unistd.h>
#include "async_command.hh"
#include "piped_async_command.hh"
#include "debug/rsdebuglevel2.h"


#define BUFFSIZE 256

/// @brief coro in charge of information handling. It takes the received states, merges it and return the updated status using the socket.
/// @param socket
/// @return true if everything goes fine
std::task<bool> inside_loop(Socket &socket)
{
    char socbuffer[BUFFSIZE] = {0};
    // TODO: lo que no entra en el buffer se procesa como otro mensaje...
    ssize_t nbRecv = co_await socket.recv(socbuffer, (sizeof socbuffer) - 1);
    if (nbRecv <= 0)
    {
        co_return false;
    }
    RS_DBG0("")<< "RECIVING (" << socbuffer << "):" << std::endl;
    std::array<char, BUFFSIZE> buffer;
    std::string merged;
    std::string cmd = "sleep 1 && echo '" + std::string(socbuffer) + "'";
    std::unique_ptr<PipedAsyncCommand> asyncecho = std::make_unique<PipedAsyncCommand>("cat", &socket);
    co_await asyncecho->writepipe(socbuffer, nbRecv);
    RS_DBG0("")<< "writepipe (" << socbuffer << "):" << std::endl;
    co_await asyncecho->readpipe(buffer.data(), BUFFSIZE);
    merged = buffer.data();
    RS_DBG0("")<< "readpipe (" << merged << "):" << std::endl;
    asyncecho.reset(nullptr);
    // problema de manejo de errores... que pasa cuando se cuelgan los endpoints y ya no reciben.
    // sin esta linea se genera un enter que no se recibe y el programa explota
    merged.erase(std::remove(merged.begin(), merged.end(), '\n'), merged.cend());
    size_t nbSend = 0;
    while (nbSend < merged.size()) // probar y hacer un pull request al creador
    {
        RS_DBG0("")<< "SENDING (" << merged << "):" << std::endl;
        ssize_t res = co_await socket.send(&(merged.data()[nbSend]), merged.size() - nbSend);
        if (res <= 0)
        {
            RS_DBG0("")<< "DONE (" << nbRecv << "):" << std::endl;
            co_return false;
        }
        nbSend += res;
    }
    // TODO: esto va al std error ?? SERA QUE PODEMOS USAR UNA LIBRERIA DE LOGGFILE
    RS_DBG0("")<< "DONE (" << nbRecv << "):" << std::endl;
    co_return false;
}

// TODO: Use more descriptive name
std::task<bool> client_socket_handler(std::unique_ptr<Socket> socket)
{
    bool run = true;
    while (run)
    {
        RS_DBG0("")<< "BEGIN";
        run = co_await inside_loop(*socket);
        RS_DBG0("")<< "END";
    }
    socket.reset(nullptr);
    co_return true;
}

std::task<> accept(Socket &listen)
{
	while(true)
	{
		RS_DBG0("")<< "begin accept";
		auto socket = co_await listen.accept();

		/* Going out of scope the returned task is destroyed, we need to
		 * detach the coroutine oterwise it will be abruptly stopped too before
		 * finishing the job */
		client_socket_handler(std::move(socket)).detach();

		RS_DBG0("")<< "end accept";
	}
}

int main()
{
    IOContext io_context{};
    Socket listen{"3490", io_context};
    auto t = accept(listen);
    t.resume();
    io_context.run();
}
