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

	auto constexpr rFAILURE = false;
	auto constexpr rSUCCESS = true;

	SharedState::NetworkMessage netMessage;
	netMessage.mTypeName = dataTypeName;

	if(! co_await getState(dataTypeName, netMessage.mData, ioContext, errbub))
		co_return false;

	auto tSocket = co_await ConnectingSocket::connect(
	            peerAddr, ioContext, errbub);
	if(!tSocket) co_return false;

#if 0
	!! CAPTURING LAMBDAS THAT ARE COROUTINES BREAKS !!
	https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rcoro-capture
	Use a macro instead even if less elegant

	auto rSocketCleanup = [&](bool isSuccess) -> std::task<bool>
	{
		/* If there is a failure even closing a socket or terminating a child
		 * process there isn't much we can do, so let downstream function report
		 * the error and terminate the process */
		co_await ioContext.closeAFD(tSocket);
		co_return isSuccess;
	};
#else
/* If there is a failure even closing a socket or terminating a child
 * process there isn't much we can do, so let downstream function report
 * the error and terminate the process */
#	define syncWithPeer_clean_socket() \
do \
{ \
	co_await ioContext.closeAFD(tSocket); \
	RS_DBG2("IOContext status after clenup: ", ioContext); \
} \
while(false)
#endif

	auto sentMessageSize = netMessage.mData.size();

	auto totalSent = co_await
	        SharedState::sendNetworkMessage(*tSocket, netMessage, errbub);
	if(totalSent == -1)
	{
		syncWithPeer_clean_socket();
		co_return rFAILURE;
	}

	auto totalReceived = co_await
	        SharedState::receiveNetworkMessage(*tSocket, netMessage, errbub);
	if(totalReceived == -1)
	{
		syncWithPeer_clean_socket();
		co_return rFAILURE;
	}

	if(! co_await mergeSlice(
			netMessage.mTypeName, netMessage.mData, ioContext, errbub ))
	{
		syncWithPeer_clean_socket();
		co_return rFAILURE;
	}

	syncWithPeer_clean_socket();

	// TODO: Add elapsed time, data trasfer bandwhidt estimation, peerAddr
	RS_INFO( /*"Synchronized with peer: ", peerAddr,*/
	         " Sent message type: ", dataTypeName,
	         " Sent message size: ", sentMessageSize,
	         " Received message type: ", netMessage.mTypeName,
	         " Received message size: ", netMessage.mData.size(),
	         " Total sent bytes: ", totalSent,
	         " Total received bytes: ", totalReceived );

	co_return rSUCCESS;
}

std::task<ssize_t> SharedState::receiveNetworkMessage(
    Socket& pSocket, NetworkMessage& networkMessage,
        std::error_condition* errbub )
{
  RS_DBG4(pSocket);

	int constexpr rFailure = -1;


	ssize_t totalReceivedBytes = 0;
	ssize_t recvRet = -1;

	networkMessage.mTypeName.clear();
	networkMessage.mData.clear();

	uint8_t dataTypeNameLenght = 0;
	recvRet = co_await pSocket.recv(&dataTypeNameLenght, 1, errbub);
	if(recvRet == -1) co_return rFailure;
	totalReceivedBytes += recvRet;

	RS_DBG2(pSocket, " dataTypeNameLenght: ", static_cast<int>(dataTypeNameLenght));

	if(dataTypeNameLenght < 1 || dataTypeNameLenght > DATA_TYPE_NAME_MAX_LENGHT)
	{
		rs_error_bubble_or_exit(
		            std::errc::bad_message, errbub,
		            " ", pSocket,
		            " Got data type name invalid lenght: ",
		            static_cast<int>(dataTypeNameLenght) );
		co_return rFailure;
	}

	networkMessage.mTypeName.resize(dataTypeNameLenght, static_cast<char>(0));
	recvRet = co_await pSocket.recv(
	            reinterpret_cast<uint8_t*>(networkMessage.mTypeName.data()),
	                        dataTypeNameLenght, errbub );
	if(recvRet == -1) co_return rFailure;
	totalReceivedBytes += recvRet;

	RS_DBG2(pSocket, " networkMessage.mTypeName: ", networkMessage.mTypeName);

	uint32_t dataLenght = 0;
	recvRet = co_await pSocket.recv(
	                        reinterpret_cast<uint8_t*>(&dataLenght), 4, errbub );
	if(recvRet == -1) co_return rFailure;
	totalReceivedBytes += recvRet;
	dataLenght = ntohl(dataLenght);


	if(dataLenght < 2 || dataLenght > DATA_MAX_LENGHT)
	{
		rs_error_bubble_or_exit(
		            std::errc::bad_message, errbub,
		            pSocket, " Got data invalid lenght: ", dataLenght);
		co_return rFailure;
	}

	networkMessage.mData.resize(dataLenght, 0);
	recvRet = co_await
	        pSocket.recv(
	            reinterpret_cast<uint8_t*>(networkMessage.mData.data()),
	                        dataLenght, errbub );
	if(recvRet == -1) co_return rFailure;
	totalReceivedBytes += recvRet;

	RS_DBG2( pSocket,
	         " Expected data lenght: ", dataLenght,
	         " received data bytes: ", recvRet );

	RS_DBG4( pSocket, " networkMessage.mData: ", networkMessage.mData);

	RS_DBG2( pSocket, " Total received bytes: ", totalReceivedBytes);
	co_return totalReceivedBytes;
}

std::task<ssize_t> SharedState::sendNetworkMessage(
    Socket& pSocket, const NetworkMessage& netMsg,
        std::error_condition* errbub )
{
	RS_DBG2(pSocket);

	ssize_t totalSentBytes = 0;
	ssize_t sentBytes = -1;

	uint8_t dataTypeLen = netMsg.mTypeName.length();
	sentBytes = co_await pSocket.send (&dataTypeLen, 1, errbub);
	if (sentBytes == -1) co_return -1;
	totalSentBytes += sentBytes;
	RS_DBG2( pSocket, " sent dataTypeLen: ", static_cast<int>(dataTypeLen),
	         " sentBytes: ", sentBytes );

	sentBytes = co_await pSocket.send(
	            reinterpret_cast<const uint8_t*>(netMsg.mTypeName.data()),
	            dataTypeLen, errbub );
	if (sentBytes == -1) co_return -1;
	totalSentBytes += sentBytes;
	RS_DBG2( pSocket, " sent netMsg.mTypeName: ", netMsg.mTypeName,
	         " sentBytes: ", sentBytes);

	uint32_t dataTypeLenNetOrder = htonl(netMsg.mData.size());
	sentBytes = co_await pSocket.send(
	    reinterpret_cast<uint8_t*>(&dataTypeLenNetOrder), 4, errbub );
	if (sentBytes == -1) co_return -1;
	totalSentBytes += sentBytes;
	RS_DBG2( pSocket, " sent netMsg.mData.size(): ", netMsg.mData.size(),
	         " sentBytes: ", sentBytes );

	sentBytes = co_await pSocket.send(
	            reinterpret_cast<const uint8_t*>(netMsg.mData.data()),
	            netMsg.mData.size(), errbub );
	if (sentBytes == -1) co_return -1;
	totalSentBytes += sentBytes;

	RS_DBG4( pSocket, " sent netMsg.mData: ", netMsg.mData);

	RS_DBG2( pSocket, " Total bytes sent: ", totalSentBytes );
	co_return totalSentBytes;
}

std::task<bool> SharedState::handleReqSyncConnection(
    std::shared_ptr<Socket> pSocket,
        std::error_condition* errbub )
{
	auto constexpr rFAILURE = false;
	auto constexpr rSUCCESS = true;
	auto& ioContext = pSocket->getIOContext();

/* !! CAPTURING LAMBDAS THAT ARE COROUTINES BREAKS !!
 * https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rcoro-capture
 * Use a macro instead even if less elegant
 */

/* If there is a failure even closing a socket or terminating a child
 * process there isn't much we can do, so let downstream function report
 * the error and terminate the process */
#define handleReqSyncConnection_clean_socket() \
do \
{ \
	co_await ioContext.closeAFD(pSocket); \
	RS_DBG2("IOContext status after clenup: ", ioContext); \
} \
while(false)

	NetworkMessage networkMessage;

	std::error_condition recvErrc;
	auto totalReceived = co_await
	        receiveNetworkMessage(*pSocket, networkMessage, &recvErrc);
	if(totalReceived < 0)
	{
		RS_INFO("Got invalid data from client ", *pSocket);
		handleReqSyncConnection_clean_socket();
		co_return rFAILURE;
	}

	auto receivedMessageSize = networkMessage.mData.size();

	std::string cmd(SHARED_STATE_LUA_CMD);
	cmd += " reqsync " + networkMessage.mTypeName;

	std::error_condition tLSHErr;
	std::shared_ptr<AsyncCommand> luaSharedState =
	        AsyncCommand::execute(cmd, ioContext, &tLSHErr);
	if(! luaSharedState)
	{
		rs_error_bubble_or_exit(tLSHErr, errbub, "Failure executing: ", cmd);
		handleReqSyncConnection_clean_socket();
		co_return rFAILURE;
	}

/* If there is a failure even closing a socket or terminating a child
 * process there isn't much we can do, so let downstream function report
 * the error and terminate the process */
#define handleReqSyncConnection_clean_socket_cmd() \
do \
{ \
	co_await ioContext.closeAFD(pSocket); \
	co_await AsyncCommand::waitTermination(luaSharedState); \
	RS_DBG2("IOContext status after clenup: ", ioContext); \
} \
while(false)

	if(co_await luaSharedState->writeStdIn(
	            networkMessage.mData.data(), networkMessage.mData.size(),
				&tLSHErr ) == -1) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
			tLSHErr, errbub, "Failure writing ", networkMessage.mData.size(),
			" bytes to ", cmd, " stdin ", tLSHErr );
		handleReqSyncConnection_clean_socket_cmd();
		co_return rFAILURE;
	}

	/* shared-state reqsync keeps reading until it get EOF, so we need to close
	 * its stdin once we finish writing so it can process the data and then
	 * return */
	if(!co_await luaSharedState->closeStdIn(errbub))
	{
		handleReqSyncConnection_clean_socket_cmd();
		co_return rFAILURE;
	}

	networkMessage.mData.clear();
	networkMessage.mData.resize(DATA_MAX_LENGHT, static_cast<char>(0));

	auto totalReadBytes = co_await luaSharedState->readStdOut(
					networkMessage.mData.data(), DATA_MAX_LENGHT, errbub );
	if(totalReadBytes == -1) RS_UNLIKELY
	{
		handleReqSyncConnection_clean_socket_cmd();
		co_return rFAILURE;
	}

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

	auto totalSent = co_await sendNetworkMessage(*pSocket, networkMessage, errbub);
	if(totalSent == -1)
	{
		handleReqSyncConnection_clean_socket_cmd();
		co_return rFAILURE;
	}

	// TODO: Add elapsed time, data trasfer bandwhidt estimation, peer address
	RS_INFO( *pSocket,
			" Received message type: ", networkMessage.mTypeName,
			" Received message size: ", receivedMessageSize,
			" Sent message size: ", networkMessage.mData.size(),
			" Total sent bytes: ", totalSent,
			" Total received bytes: ", totalReceived );

	handleReqSyncConnection_clean_socket_cmd();

	co_return rSUCCESS;
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
	if(! co_await AsyncCommand::waitTermination(getCandidatesCmd, errbub))
		co_return false;

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

/*static*/ std::task<bool> SharedState::getState(
	const std::string& dataTypeName,
	std::vector<uint8_t>& dataStorage,
	IOContext& ioContext,
	std::error_condition* errbub )
{
	dataStorage.clear();

	std::shared_ptr<AsyncCommand> getStateCmd =
		AsyncCommand::execute(
			std::string(SHARED_STATE_LUA_CMD) + " get " + dataTypeName,
			ioContext, errbub );
	if(!getStateCmd) co_return false;

	dataStorage.resize(DATA_MAX_LENGHT);
	auto totalReadBytes = co_await getStateCmd->readStdOut(
		dataStorage.data(), DATA_MAX_LENGHT, errbub );
	if(totalReadBytes == -1) co_return false;
	dataStorage.resize(totalReadBytes);

	if(! co_await AsyncCommand::waitTermination(getStateCmd, errbub))
		co_return false;

	co_return true;
}

/*static*/ std::task<bool> SharedState::mergeSlice(
	const std::string& dataTypeName,
	const std::vector<uint8_t>& dataSlice,
	IOContext& ioContext,
	std::error_condition* errbub )
{
	std::string cmdMerge(SHARED_STATE_LUA_CMD);
	cmdMerge += " reqsync " + dataTypeName;

	auto luaSharedState = AsyncCommand::execute(
		cmdMerge, ioContext, errbub );
	if(!luaSharedState) co_return false;

	bool retval = -1 != co_await luaSharedState->writeStdIn(
		dataSlice.data(), dataSlice.size(), errbub );

	/* shared-state keeps reading until it get EOF, so we need to close
	 * its stdin once we finish writing so it can process the data and then
	 * return */
	retval &= co_await luaSharedState->closeStdIn(errbub);
	retval &= co_await AsyncCommand::waitTermination(luaSharedState, errbub);

	co_return retval;
}
