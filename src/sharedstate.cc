/*
 * Shared State
 *
 * Copyright (C) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include <chrono>
#include <algorithm>
#include <arpa/inet.h>
#include <sstream>

#include <util/rsnet.h>

#include "sharedstate.hh"
#include "socket.hh"
#include "file_read_operation.hh"
#include "async_command.hh"

#include <util/rsdebug.h>
#include <util/rserrorbubbleorexit.h>
#include <util/rsdebuglevel2.h>

std::task<bool> SharedState::syncWithPeer(
        std::string dataTypeName, const sockaddr_storage& peerAddr,
        IOContext& ioContext, std::error_condition* errbub )
{
	RS_DBG2(dataTypeName, " ", sockaddr_storage_tostring(peerAddr), " ", errbub);

	SharedState::NetworkMessage netMessage;
	netMessage.mTypeName = dataTypeName;

	std::string cmdGet(SHARED_STATE_LUA_CMD);
	cmdGet += " get " + netMessage.mTypeName;

	std::shared_ptr<AsyncCommand> luaSharedState =
	        AsyncCommand::execute(cmdGet, ioContext, errbub);
	if(!luaSharedState) co_return false;
	RS_DBG2(cmdGet, " running at: ", *luaSharedState);

	netMessage.mData.clear();
	netMessage.mData.resize(DATA_MAX_LENGHT, static_cast<char>(0));

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
	while(nbRecvFromPipe);
	netMessage.mData.resize(totalReadBytes);

	if(! co_await AsyncCommand::waitTermination(
	            luaSharedState, errbub )) co_return false;

	auto tSocket = co_await ConnectingSocket::connect(
	            peerAddr, ioContext, errbub);
	if(!tSocket) co_return false;

	auto sentMessageSize = netMessage.mData.size();

	// TODO: deal with errors
	auto totalSent = co_await
	        SharedState::sendNetworkMessage(*tSocket, netMessage, errbub);

	// TODO: deal with errors
	auto totalReceived = co_await
	        SharedState::receiveNetworkMessage(*tSocket, netMessage, errbub);

	std::string cmdMerge(SHARED_STATE_LUA_CMD);
	cmdMerge += " reqsync " + netMessage.mTypeName;


	// TODO: deal with errors
	luaSharedState = AsyncCommand::execute(
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

	co_await AsyncCommand::waitTermination(luaSharedState);

	// TODO: Add elapsed time, data trasfer bandwhidt estimation, peerAddr
	RS_INFO( /*"Synchronized with peer: ", peerAddr,*/
	         " Sent message type: ", dataTypeName,
	         " Sent message size: ", sentMessageSize,
	         " Received message type: ", netMessage.mTypeName,
	         " Received message size: ", netMessage.mData.size(),
	         " Total sent bytes: ", totalSent,
	         " Total received bytes: ", totalReceived );

	co_return true;
}

std::task<int> SharedState::receiveNetworkMessage(
        Socket& socket, NetworkMessage& networkMessage,
        std::error_condition* errbub )
{
	int constexpr rFailure = -1;
	RS_DBG4(socket);
	// TODO: define and use proper error_conditions to return
	// TODO: deal with socket errors

	int receivedBytes = 0;
	int recvRet = -1;

	networkMessage.mTypeName.clear();
	networkMessage.mData.clear();


	uint8_t dataTypeNameLenght = 0;
	recvRet = co_await socket.recv(&dataTypeNameLenght, 1, errbub);
	if(recvRet == -1) co_return rFailure;
	receivedBytes += recvRet;

	RS_DBG2(socket, " dataTypeNameLenght: ", static_cast<int>(dataTypeNameLenght));

	if(dataTypeNameLenght < 1 || dataTypeNameLenght > DATA_TYPE_NAME_MAX_LENGHT)
	{
		rs_error_bubble_or_exit(
		            std::errc::bad_message, errbub,
		            " ", socket,
		            " Got data type name invalid lenght: ",
		            static_cast<int>(dataTypeNameLenght) );
		co_return rFailure;
	}

	networkMessage.mTypeName.resize(dataTypeNameLenght, static_cast<char>(0));
	recvRet = co_await socket.recv(
	            reinterpret_cast<uint8_t*>(networkMessage.mTypeName.data()),
	            dataTypeNameLenght );
	if(recvRet == -1) co_return rFailure;
	receivedBytes += recvRet;

	RS_DBG2(socket, " networkMessage.mTypeName: ", networkMessage.mTypeName);

	uint32_t dataLenght = 0;
	recvRet = co_await socket.recv(
	            reinterpret_cast<uint8_t*>(&dataLenght), 4 );
	if(recvRet == -1) co_return rFailure;
	receivedBytes += recvRet;
	dataLenght = ntohl(dataLenght);


	if(dataLenght < 2 || dataLenght > DATA_MAX_LENGHT)
	{
		rs_error_bubble_or_exit(
		            std::errc::bad_message, errbub,
		            socket, " Got data invalid lenght: ", dataLenght);
		co_return rFailure;
	}

	networkMessage.mData.resize(dataLenght, 0);
	recvRet = co_await
	        socket.recv(
	            reinterpret_cast<uint8_t*>(networkMessage.mData.data()),
	            dataLenght );
	if(recvRet == -1) co_return rFailure;
	receivedBytes += recvRet;

	RS_DBG2( socket,
	         " Expected data lenght: ", dataLenght,
	         " received data bytes: ", recvRet );

	RS_DBG4( socket, " networkMessage.mData: ", networkMessage.mData);

	RS_DBG2( socket, " Total received bytes: ", receivedBytes);
	co_return receivedBytes;
}

std::task<int> SharedState::sendNetworkMessage(
        Socket& socket, const NetworkMessage& netMsg,
        std::error_condition* errbub )
{
	RS_DBG2(socket);

	int sentBytes = 0;

	uint8_t dataTypeLen = netMsg.mTypeName.length();
	sentBytes += co_await socket.send(&dataTypeLen, 1);

	RS_DBG2(socket, " sent dataTypeLen: ", static_cast<int>(dataTypeLen));

	sentBytes += co_await socket.send(
	            reinterpret_cast<const uint8_t*>(netMsg.mTypeName.data()),
	            dataTypeLen );

	RS_DBG2( socket, " sent netMsg.mTypeName: ", netMsg.mTypeName );

	uint32_t dataTypeLenNetOrder = htonl(netMsg.mData.size());
	sentBytes += co_await socket.send(
	            reinterpret_cast<uint8_t*>(&dataTypeLenNetOrder), 4);

	RS_DBG2( socket, " sent netMsg.mData.size(): ", netMsg.mData.size() );

	sentBytes += co_await socket.send(
	            reinterpret_cast<const uint8_t*>(netMsg.mData.data()),
	            netMsg.mData.size() );

	RS_DBG4( socket, " sent netMsg.mData: ", netMsg.mData);

	RS_DBG2( socket, " Total bytes sent: ", sentBytes );
	co_return sentBytes;
}

std::task<bool> SharedState::handleReqSyncConnection(
        std::shared_ptr<Socket> socket,
        std::error_condition* errbub )
{
	NetworkMessage networkMessage;

	std::error_condition recvErrc;
	auto totalReceived = co_await
	        receiveNetworkMessage(*socket, networkMessage, &recvErrc);
	if(totalReceived < 0)
	{
		RS_INFO("Got invalid data from client ", *socket);
		co_await socket->getIOContext().closeAFD(socket);
		co_return false;
	}

	auto receivedMessageSize = networkMessage.mData.size();

#ifdef GIO_DUMMY_TEST
	std::string cmd = "cat /tmp/shared-state/data/" +
	        networkMessage.mTypeName + ".json";
#else
	std::string cmd(SHARED_STATE_LUA_CMD);
	cmd += " reqsync " + networkMessage.mTypeName;
#endif

	std::error_condition tLSHErr;

	// TODO: gracefully deal with errors
	std::shared_ptr<AsyncCommand> luaSharedState =
	        AsyncCommand::execute(cmd, socket->getIOContext());

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

	/* shared-state keeps reading until it get EOF, so we need to close the
	 * its stdin once we finish writing so it can process the data and then
	 * return */
	co_await luaSharedState->closeStdIn();

	networkMessage.mData.clear();
	networkMessage.mData.resize(DATA_MAX_LENGHT, static_cast<char>(0));

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

		RS_DBG0( luaSharedState,
		         " nbRecvFromPipe: ", nbRecvFromPipe,
		         " data read >>>", justRecv, "<<<" );
	}
	while(nbRecvFromPipe);
	co_await luaSharedState->closeStdOut();

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

	co_await AsyncCommand::waitTermination(luaSharedState);

	auto totalSent = co_await sendNetworkMessage(*socket, networkMessage);

	co_await socket->getIOContext().closeAFD(socket);

	// TODO: Add elapsed time, data trasfer bandwhidt estimation, peer address
	RS_INFO( *socket,
	         " Received message type: ", networkMessage.mTypeName,
	         " Received message size: ", receivedMessageSize,
	         " Sent message size: ", networkMessage.mData.size(),
	         " Total sent bytes: ", totalSent,
	         " Total received bytes: ", totalReceived );

	co_return false;
}

std::task<bool> SharedState::getCandidatesNeighbours(
        std::vector<sockaddr_storage>& peerAddresses,
        IOContext& ioContext, std::error_condition* errbub )
{
	peerAddresses.clear();

	std::shared_ptr<AsyncCommand> getCandidatesCmd =
	        AsyncCommand::execute(
	            std::string(SHARED_STATE_GET_CANDIDATES_CMD), ioContext );

	std::stringstream neigStrStream;

	ssize_t numReadBytes = 0;
	ssize_t totalReadBytes = 0;
	do
	{
		constexpr size_t maxRecv = 1000;
		std::string justRecv(maxRecv, char(0));
		numReadBytes = co_await getCandidatesCmd->readStdOut(
		            reinterpret_cast<uint8_t*>(justRecv.data()), maxRecv);
		justRecv.resize(numReadBytes);
		totalReadBytes += numReadBytes;
		neigStrStream << justRecv;
	}
	while(numReadBytes);
	co_await getCandidatesCmd->closeStdOut();
	co_await AsyncCommand::waitTermination(getCandidatesCmd);

#ifdef SS_OPENWRT_CMD_LEAK_WORKAROUND
	/* When running on OpenWrt the execvp command line argv[0] is read as first
	 * line of the stdout content, we need to discard it.
	 * TODO: investigate why this is happening, and if a better way to deal with
	 * it exists */
	{
		std::string discardFirstLine;
		std::getline(neigStrStream, discardFirstLine);
	}
#endif // def SS_OPENWRT_BUILD

	for (std::string candLine; std::getline(neigStrStream, candLine); )
	{
		sockaddr_storage peerAddr;
		if(!sockaddr_storage_inet_pton(peerAddr, candLine))
		{
			rs_error_bubble_or_exit(
			            std::errc::bad_address, errbub,
			            "Invalid peer address: ", candLine );
			co_return false;
		}

		sockaddr_storage_setport(peerAddr, TCP_PORT);
		peerAddresses.push_back(peerAddr);
	}

	RS_DBG2( "Found ", peerAddresses.size(), " potential neighbours" );
#if RS_DEBUG_LEVEL > 1
	for( auto&& peerAddr : std::as_const(peerAddresses))
		RS_DBG(sockaddr_storage_iptostring(peerAddr));
#endif // RS_DEBUG_LEVEL

	co_return true;
}
