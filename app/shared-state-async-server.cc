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
#include <signal.h>

#ifdef SS_OPENWRT_CMD_LEAK_WORKAROUND
#	include <algorithm>
#endif

#include "io_context.hh"
#include "socket.hh"
#include "task.hh"
#include "sharedstate.hh"
#include "piped_async_command.hh"
#include "sharedstate.hh"

#include <util/stacktrace.h>
#include <util/rsdebuglevel2.h>

static CrashStackTrace gCrashStackTrace;

using namespace SharedState;

/**
 * @brief this task handles each message. It takes receives a message,
 * calls async cat to implement an echo and returns the same message
 * using the same socket.
 * @param socket
 * @return std::task<bool> returns true if it has not finished or false
 * to finalize the socket and the communication.
 */
std::task<bool> echo_loop(std::shared_ptr<Socket> socket)
{
	NetworkMessage networkMessage;

	auto totalReceived = co_await
	        receiveNetworkMessage(*socket, networkMessage);
	auto receivedMessageSize = networkMessage.mData.size();

#ifdef GIO_DUMMY_TEST
	std::string cmd = "cat /tmp/shared-state/data/" +
	        networkMessage.mTypeName + ".json";
#else
	std::string cmd = "/usr/bin/lua /usr/bin/shared-state reqsync";
	cmd = cmd + " " + networkMessage.mTypeName;
#endif

	std::error_condition tLSHErr;

	// TODO: gracefully deal with errors
	std::shared_ptr<PipedAsyncCommand> luaSharedState =
	        PipedAsyncCommand::execute(cmd, socket->getIOContext());

	if(co_await luaSharedState->writeStdIn(
	            networkMessage.mData.data(), networkMessage.mData.size(),
	            &tLSHErr ) == -1)
	{
		RS_ERR("Failure writing ", networkMessage.mData.size(), " bytes ",
		       " to LSH stdin ", tLSHErr );
		co_await luaSharedState->getIOContext().closeAFD(luaSharedState);
		co_await socket->getIOContext().closeAFD(socket);
		co_return false;
	}

	co_await luaSharedState->finishwriting();

	networkMessage.mData.clear();
	networkMessage.mData.resize(DATA_MAX_LENGHT, static_cast<char>(0));

	/* Some applications keep reading until EOF is sent, the only way to ensure
	 * termination is closing the write end */
	ssize_t rec_ammount = 0;
	ssize_t nbRecvFromPipe = 0;
	int totalReadBytes = 0;
	auto dataPtr = networkMessage.mData.data();
	do
	{
		nbRecvFromPipe = co_await luaSharedState->readStdOut(
		            dataPtr + totalReadBytes, DATA_MAX_LENGHT - totalReadBytes);
		std::string justRecv(
		            reinterpret_cast<char*>(dataPtr + totalReadBytes),
		            nbRecvFromPipe );
		totalReadBytes += nbRecvFromPipe;

		RS_DBG0( *socket,
		         " nbRecvFromPipe: ", nbRecvFromPipe,
		         ", done reading? ", luaSharedState->doneReading(),
		         " data read >>>", justRecv, "<<<" );
	}
	while ((nbRecvFromPipe != 0) && !luaSharedState->doneReading() );

	/* Reading from this pipe in OpenWrt and lua shared-state never returns 0 it
	 * just returns -1 and the donereading flag is always 0
	 * it seems that the second end of line can be a good candidate for end of
	 * transmission */
	co_await luaSharedState->finishReading();

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
	 * TODO: investigate why this is happening, and if a better way to deal with
	 * it exists */
	{
		auto& mData = networkMessage.mData;
		auto&& cmdEnd = std::find(mData.begin(), mData.end(), '\n');
		mData.erase(mData.begin(), ++cmdEnd);
	}
#endif // def SS_OPENWRT_BUILD

	co_await PipedAsyncCommand::waitForProcessTermination(luaSharedState);

	auto totalSent = co_await sendNetworkMessage(*socket, networkMessage);

	co_await socket->getIOContext().closeAFD(socket);

	RS_DBG2( socket,
	         " Received message type: ", networkMessage.mTypeName,
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
std::task<bool> client_socket_handler(std::shared_ptr<Socket> socket)
{
	// TODO:can be std::task<void> no need to use bool
	bool run = true;

	while (run) // TODO: Probably not needed
	{
		RS_DBG0("BEGIN ", socket);

		run = co_await echo_loop(socket);

		// mFD should be -1 at this point
		RS_DBG0("END ", socket);
	}

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
		client_socket_handler(socket).detach();
	}
}

int main()
{
	/* We expect write failures, expecially on sockets, to occur but we want to
	 * handle them where the error occurs rather than in a SIGPIPE handler */
	signal(SIGPIPE, SIG_IGN);

	auto ioContext = IOContext::setup();
	auto listener = ListeningSocket::setupListener(3490, *ioContext.get());

	RS_INFO("Created listening socket ", *listener);

	auto t = acceptConnections(*listener.get());
	t.resume();
	ioContext->run();
}
