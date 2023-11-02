/*
 * Shared State
 *
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
#include "file_read_operation.hh"
#include "sharedstate.hh"
#include "piped_async_command.hh"

#include <cstdio>
#include <cstdlib>
#include <arpa/inet.h>
#include <signal.h>

#include <util/rsdebug.h>
#include <util/rsdebuglevel2.h>
#include <util/rsnet.h>
#include <util/stacktrace.h>

static CrashStackTrace gCrashStackTrace;

using namespace SharedState;

std::task<bool> syncWithPeer(
        std::string dataTypeName, const sockaddr_storage& peerAddr,
        IOContext& ioContext, std::error_condition* errbub = nullptr )
{
	SharedState::NetworkMessage netMessage;
	netMessage.mTypeName = dataTypeName;

	std::string cmdGet = "/usr/bin/shared-state get ";
	cmdGet += netMessage.mTypeName;

	std::shared_ptr<PipedAsyncCommand> luaSharedState =
	        PipedAsyncCommand::execute(cmdGet, ioContext, errbub);
	if(!luaSharedState) co_return false;

	netMessage.mData.clear();
	netMessage.mData.resize(DATA_MAX_LENGHT, static_cast<char>(0));

	ssize_t rec_ammount = 0;
	ssize_t nbRecvFromPipe = 0;
	int totalReadBytes = 0;
	auto dataPtr = netMessage.mData.data();
	do
	{
		// TODO: deal with errors
		nbRecvFromPipe = co_await luaSharedState->readStdOut(
		            dataPtr + totalReadBytes, DATA_MAX_LENGHT - totalReadBytes);
		std::string justRecv(
		            reinterpret_cast<char*>(dataPtr + totalReadBytes),
		            nbRecvFromPipe );
		totalReadBytes += nbRecvFromPipe;

		RS_DBG4( luaSharedState,
		         " nbRecvFromPipe: ", nbRecvFromPipe,
		         " data read >>>", justRecv, "<<<" );
	}
	while (nbRecvFromPipe);
	netMessage.mData.resize(totalReadBytes);

	// TODO: deal with errors
	co_await PipedAsyncCommand::waitForProcessTermination(
	            luaSharedState, errbub );

	// TODO: deal with errors
	auto tSocket = co_await ConnectingSocket::connect(
	            peerAddr, ioContext, errbub);
	auto sentMessageSize = netMessage.mData.size();

	// TODO: deal with errors
	auto totalSent = co_await
	        SharedState::sendNetworkMessage(*tSocket, netMessage, errbub);

	// TODO: deal with errors
	auto totalReceived = co_await
	        SharedState::receiveNetworkMessage(*tSocket, netMessage, errbub);

	std::string cmdMerge = "/usr/bin/shared-state reqsync ";
	cmdMerge += netMessage.mTypeName;


	// TODO: deal with errors
	luaSharedState = PipedAsyncCommand::execute(
	            cmdMerge, tSocket->getIOContext(), errbub );

	if(co_await luaSharedState->writeStdIn(
	            netMessage.mData.data(), netMessage.mData.size(),
	            errbub ) == -1)
	{
		co_await luaSharedState->getIOContext().closeAFD(luaSharedState);
		co_await tSocket->getIOContext().closeAFD(tSocket);
		co_return false;
	}

	/* shared-state keeps reading until it get EOF, so we need to close the
	 * its stdin once we finish writing so it can process the data and then
	 * return */
	co_await luaSharedState->closeStdIn();

	co_await PipedAsyncCommand::waitForProcessTermination(luaSharedState);

	// TODO: Add elapsed time, data trasfer bandwhidt estimation
	RS_INFO( "Synchronized with peer: ", peerAddr,
	         " Sent message type: ", dataTypeName,
	         " Sent message size: ", sentMessageSize,
	         " Received message type: ", netMessage.mTypeName,
	         " Received message size: ", netMessage.mData.size(),
	         " Total sent bytes: ", totalSent,
	         " Total received bytes: ", totalReceived );

	co_return true;
}

int main(int argc, char* argv[])
{
	if(argc < 3)
	{
		RS_FATAL("Need type name and peer IP address");
		return -EINVAL;
	}

	std::string dataTypeName(argv[1]);
	std::string peerAddrStr(argv[2]);

	sockaddr_storage peerAddr;
	if(!sockaddr_storage_inet_pton(peerAddr, peerAddrStr))
	{
		RS_FATAL("Invalid IP address");
		return -static_cast<int>(std::errc::bad_address);
	}
	sockaddr_storage_setport(peerAddr, 3490);

	RS_INFO("Got dataTypeName: ", dataTypeName, " peerAddrStr: ", peerAddrStr);

	/* We expect write failures, expecially on sockets, to occur but we want to
	 * handle them where the error occurs rather than in a SIGPIPE handler */
	signal(SIGPIPE, SIG_IGN);

	auto ioContext = IOContext::setup();
	auto sendTask = syncWithPeer(dataTypeName, peerAddr, *ioContext.get());
	sendTask.resume();
	ioContext->run();

	return 0;
}
