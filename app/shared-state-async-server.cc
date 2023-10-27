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

#include <unistd.h>

#include "io_context.hh"
#include "socket.hh"
#include "task.hh"
#include "sharedstate.hh"
#include "piped_async_command.hh"
#include "sharedstate.hh"

#include <util/stacktrace.h>
#include <util/rsdebuglevel2.h>

static CrashStackTrace gCrashStackTrace;

static constexpr int BUFFSIZE = 3048;

using namespace SharedState;

/**
 * @brief this task handles each message. It takes receives a message,
 * calls async cat to implement an echo and returns the same message
 * using the same socket.
 * @param socket
 * @return std::task<bool> returns true if it has not finished or false
 * to finalize the socket and the communication.
 */
std::task<bool> echo_loop(Socket& socket)
{
	NetworkMessage networkMessage;

	auto totalReceived = co_await
	        receiveNetworkMessage(socket, networkMessage);
	auto receivedMessageSize = networkMessage.mData.size();

#ifdef GIO_DUMMY_TEST
	std::string cmd = "cat /home/gio/Builds/gomblot.json";
#else
	std::string cmd = "/usr/bin/lua /usr/bin/shared-state reqsync";
	cmd = cmd + " " + networkMessage.mTypeName;
#endif

	// TODO: gracefully deal with errors
	std::unique_ptr<PipedAsyncCommand> luaSharedState =
	        PipedAsyncCommand::execute(cmd, socket.io_context_);

	co_await luaSharedState->writepipe(
	            reinterpret_cast<const uint8_t*>(networkMessage.mData.data()),
	            networkMessage.mData.size() );
	luaSharedState->finishwriting();

	networkMessage.mData.clear();
	networkMessage.mData.resize(DATA_MAX_LENGHT, static_cast<char>(0));

	/* Some applications keep reading until EOF is sent, the only way to ensure
	 * termination is closing the write end */
	ssize_t rec_ammount = 0;
	ssize_t nbRecvFromPipe = 0;
	int endlconuter = 0;
	int totalReadBytes = 0;
	uint8_t* dataPtr = reinterpret_cast<uint8_t*>(networkMessage.mData.data());
	do
	{
		nbRecvFromPipe = co_await luaSharedState->readpipe(
		            dataPtr + totalReadBytes, DATA_MAX_LENGHT - totalReadBytes);
		totalReadBytes += nbRecvFromPipe;

		// TODO: shouldn't it be == ?
		if (*dataPtr != '\n') endlconuter++;

		RS_DBG0( "nbRecvFromPipe: ", nbRecvFromPipe,
		         ", done reading? ", luaSharedState->doneReading(),
		         " endlconuter: ", endlconuter);
	}
	while (
	       (nbRecvFromPipe != 0) &&
	       (!luaSharedState->doneReading()) &&
	       endlconuter != 1
	      );

	/* Reading from this pipe in OpenWrt and lua shared-state never returns 0 it
	 * just returns -1 and the donereading flag is always 0
	 * it seems that the second end of line can be a good candidate for end of
	 * transmission */
	luaSharedState->finishReading();

	/* Truncate data size to necessary. Avoid sending millions of zeros around.
	 *
	 * While testing on my Gentoo machine, I noticed that printing to the
	 * terminal networkMessage.mData seemed to be extremely costly to the point
	 * to keep my CPU usage at 100% for at least 20 seconds doing something
	 * unclear, even more curious the process being reported to eat the CPU was
	 * not shared-state-server but the parent console on which the process is
	 * running, in my case either Qt Creator or Konsole, even when running under
	 * gdb. When redirecting the output to either a file or /dev/null the
	 * problem didn't happen, but the created file was 1GB, aka
	 * DATA_MAX_LENGHT of that time.
	 *
	 * Similar behavior appeared if printing networkMessage.mData on the
	 * shared-state-client.
	 *
	 * The culprit wasn't that obvious at first all those nullbytes where
	 * invisible.
	 */
	networkMessage.mData.resize(totalReadBytes);

#ifdef SS_OPENWRT_CMD_LEAK_WORKAROUND
	/* When running on OpenWrt the execvp command line is read as first line
	 * of the pipe content.
	 * It happens with both shared-state lua command and with cat command.
	 * TODO: investivate why this is happening, and if a better way to deal with
	 * it exists
	 */
	networkMessage.mData.erase(0, networkMessage.mData.find('\n') + 1);
#endif // def SS_OPENWRT_BUILD

	auto totalSent = co_await sendNetworkMessage(socket, networkMessage);

	co_await luaSharedState->waitForProcessTermination();
	luaSharedState.reset(nullptr);

	RS_DBG2( "Received message type: ", networkMessage.mTypeName,
	         " Received message size: ", receivedMessageSize,
	         " Sent message size: ", networkMessage.mData.size(),
	         " Total sent bytes: ", totalSent,
	         " Total received bytes: ", totalReceived );

	co_return false;
}

/**
 * @brief Handles a client socket until the inside task finishes
 * this can enable a multi message communication over a single socket
 *
 * @param socket a socket generated by accept
 * @return std::task<bool> a task that can be resumed or detached
 */
std::task<bool> client_socket_handler(std::unique_ptr<Socket> socket)
{
    // TODO:can be std::task<void> no need to use bool
    bool run = true;
    while (run)
    {
        RS_DBG0("BEGIN");
        run = co_await echo_loop(*socket);
        RS_DBG0("END");
    }
    socket.reset(nullptr);
    co_return true;
}

std::task<> acceptConnections(ListeningSocket& listener)
{
	while(true)
	{
		auto socket = co_await listener.accept();

		/* Going out of scope the returned task is destroyed, we need to
		 * detach the coroutine otherwise it will be abruptly stopped too before
		 * finishing the job */
		client_socket_handler(std::move(socket)).detach();
	}
}

int main()
{
	auto ioContext = IOContext::setup();
	auto listener = ListeningSocket::setupListener(3490, *ioContext.get());
	auto t = acceptConnections(*listener.get());
	t.resume();
	ioContext->run();
}
