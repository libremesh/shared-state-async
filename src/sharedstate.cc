/*
 * Shared State
 *
 * Copyright (C) 2023-2024  Gioacchino Mazzurco <gio@eigenlab.org>
 * Copyright (c) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (c) 2023  Instituto Nacional de Tecnología Industrial
 * Copyright (C) 2023-2024  Asociación Civil Altermundi <info@altermundi.net>
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
#include <fstream>

#ifdef SHARED_STATE_STAT_FILE_LOCKING
#	include <fcntl.h>
#	include <sys/file.h>
#endif // def SHARED_STATE_STAT_FILE_LOCKING

#include <nlohmann/json.hpp>

#include <util/rsnet.h>
#include <util/rsurl.h>

#include "sharedstate.hh"
#include "async_socket.hh"
#include "async_command.hh"

#include <util/rsdebug.h>
#include <util/rserrorbubbleorexit.h>
#include <util/rsdebuglevel2.h>

using json = nlohmann::json;

void to_json(json& pJson, const SharedState::NetworkStats& pStat)
{
	pJson = json{
	    { "mTS", pStat.mTS.time_since_epoch().count() },
	    { "mRttExt", pStat.mRttExt.count() },
	    { "mUpBwMbsExt", pStat.mUpBwMbsExt },
	    { "mDownBwMbsExt", pStat.mDownBwMbsExt }
    };
}

void from_json(const json& pJson, SharedState::NetworkStats& pStat)
{
	pStat.mTS = std::chrono::time_point<std::chrono::steady_clock>(
	            std::chrono::time_point<std::chrono::steady_clock>::duration(
	                pJson.at("mTS").get<uint64_t>()) );

	pStat.mRttExt = std::chrono::microseconds{
	        pJson.at("mRttExt").get<uint64_t>() };

	pJson.at("mUpBwMbsExt").get_to(pStat.mUpBwMbsExt);
	pJson.at("mDownBwMbsExt").get_to(pStat.mDownBwMbsExt);
}

std::task<bool> SharedState::syncWithPeer(
        std::string dataTypeName, const sockaddr_storage& peerAddr,
        IOContext& ioContext, std::error_condition* errbub )
{
	RS_DBG3(dataTypeName, " ", sockaddr_storage_tostring(peerAddr), " ", errbub);

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
	RS_DBG3("IOContext status after clenup: ", ioContext); \
} \
while(false)
#endif

	NetworkStats netStats;
	sockaddr_storage_copy(peerAddr, netStats.mPeer);

	if(!co_await SharedState::clientHandShake(
	            *tSocket, netStats, errbub )) RS_UNLIKELY
	{
		syncWithPeer_clean_socket();
		co_return rFAILURE;
	}

	auto sentMessageSize = netMessage.mData.size();
	auto totalSent = co_await
	        SharedState::sendNetworkMessage(
	            *tSocket, netMessage, netStats, errbub );
	if(totalSent == -1)
	{
		syncWithPeer_clean_socket();
		co_return rFAILURE;
	}

	auto totalReceived = co_await
	        SharedState::receiveNetworkMessage(
	            *tSocket, netMessage, netStats, errbub );
	if(totalReceived == -1)
	{
		syncWithPeer_clean_socket();
		co_return rFAILURE;
	}

	using namespace std::chrono;
	const auto mergeBTP = steady_clock::now();
	if(! co_await mergeSlice(
			netMessage.mTypeName, netMessage.mData, ioContext, errbub ))
	{
		syncWithPeer_clean_socket();
		co_return rFAILURE;
	}
	const auto mergeETP = steady_clock::now();
	const auto mergeMuSecs = duration_cast<microseconds>(mergeETP - mergeBTP);

	syncWithPeer_clean_socket();

	RS_INFO( "Synchronized with peer: ", peerAddr,
	         " Sent message type: ", dataTypeName,
	         " Sent message size: ", sentMessageSize,
	         " Received message type: ", netMessage.mTypeName,
	         " Received message size: ", netMessage.mData.size(),
	         " Total sent bytes: ", totalSent,
	         " Total received bytes: ", totalReceived,
	         " Extimated upload BW: ", netStats.mUpBwMbsExt, "Mbit/s",
	         " Extimated download BW: ", netStats.mDownBwMbsExt, "Mbit/s",
	         " Extimated RTT: ", netStats.mRttExt.count(), "μs",
	         " Processing time: ", mergeMuSecs.count(), "μs" );

	co_return rSUCCESS && collectStat(netStats, errbub);
}

std::task<ssize_t> SharedState::receiveNetworkMessage(
        AsyncSocket& pSocket, NetworkMessage& networkMessage,
        NetworkStats& netStats, std::error_condition* errbub )
{
	RS_DBG3(pSocket);

	ssize_t constexpr rFAILURE = -1;

	ssize_t totalReceivedBytes = 0;
	ssize_t recvRet = -1;

	networkMessage.mTypeName.clear();
	networkMessage.mData.clear();

	using namespace std::chrono;
	const auto recvBTP = steady_clock::now();

	uint8_t dataTypeNameLenght = 0;
	recvRet = co_await pSocket.recv(&dataTypeNameLenght, 1, errbub);
	if(recvRet == -1) co_return rFAILURE;
	totalReceivedBytes += recvRet;

	RS_DBG3(pSocket, " dataTypeNameLenght: ", static_cast<int>(dataTypeNameLenght));

	if(dataTypeNameLenght < 1 || dataTypeNameLenght > DATA_TYPE_NAME_MAX_LENGHT)
	{
		rs_error_bubble_or_exit(
		            std::errc::bad_message, errbub,
		            " ", pSocket,
		            " Got data type name invalid lenght: ",
		            static_cast<int>(dataTypeNameLenght) );
		co_return rFAILURE;
	}

	networkMessage.mTypeName.resize(dataTypeNameLenght, static_cast<char>(0));
	recvRet = co_await pSocket.recv(
	            reinterpret_cast<uint8_t*>(networkMessage.mTypeName.data()),
	                        dataTypeNameLenght, errbub );
	if(recvRet == -1) co_return rFAILURE;
	totalReceivedBytes += recvRet;

	RS_DBG3(pSocket, " networkMessage.mTypeName: ", networkMessage.mTypeName);

	uint32_t dataLenght = 0;
	recvRet = co_await pSocket.recv(
	                        reinterpret_cast<uint8_t*>(&dataLenght), 4, errbub );
	if(recvRet == -1) co_return rFAILURE;
	totalReceivedBytes += recvRet;
	dataLenght = ntohl(dataLenght);

	if(dataLenght < 2 || dataLenght > DATA_MAX_LENGHT)
	{
		rs_error_bubble_or_exit(
		            std::errc::bad_message, errbub,
		            pSocket, " Got data invalid lenght: ", dataLenght);
		co_return rFAILURE;
	}

	networkMessage.mData.resize(dataLenght, 0);
	recvRet = co_await
	        pSocket.recv(
	            reinterpret_cast<uint8_t*>(networkMessage.mData.data()),
	                        dataLenght, errbub );
	if(recvRet == -1) co_return rFAILURE;
	totalReceivedBytes += recvRet;

	const auto recvETP = steady_clock::now();

	/* Acknowledge total received bytes, all tests without this worked fine
	 * anyway, so this has been added mainly to enable the sender to extimate
	 * sending time in user space */
	uint32_t netOrderReceivedB = htonl(totalReceivedBytes);
	auto sendRet = co_await
	        pSocket.send(
	            reinterpret_cast<uint8_t*>(&netOrderReceivedB), 4, errbub );
	if(sendRet == -1) co_return rFAILURE;

	/* RTT impact on bandwidth calculation should be usually neglegible, and for
	 * sure becomes even more neglegible when the network and hence the shared
	 * data size grows.
	 * Subtracting it causes negative result in some situations for no
	 * appreciable benefit in the rest of the cases so we don't take it in
	 * account here */
	if(recvETP > recvBTP) RS_LIKELY
	        netStats.mDownBwMbsExt = MbitPerSec(
	            totalReceivedBytes,
	            duration_cast<microseconds>(recvETP - recvBTP).count() );
	else
		RS_ERR( "Time must have wrapped during download, bandwidth extimation "
		        "ignored" );


	RS_DBG3( pSocket,
	         " Expected data lenght: ", dataLenght,
	         " received data bytes: ", recvRet );

	RS_DBG4(pSocket, " networkMessage.mData: ", networkMessage.mData);

	RS_DBG3(pSocket, " Total received bytes: ", totalReceivedBytes);
	co_return totalReceivedBytes;
}

std::task<ssize_t> SharedState::sendNetworkMessage(
        AsyncSocket& pSocket, const NetworkMessage& netMsg,
        NetworkStats& netStats, std::error_condition* errbub )
{
	RS_DBG3(pSocket, " type: ", netMsg.mTypeName, " dataLen: ", netMsg.mData.size());

	ssize_t constexpr rFAILURE = -1;

	ssize_t totalSentBytes = 0;
	ssize_t sentBytes = -1;

	using namespace std::chrono;
	const auto sendBTP = steady_clock::now();

	uint8_t dataTypeLen = netMsg.mTypeName.length();
	sentBytes = co_await pSocket.send(&dataTypeLen, 1, errbub);
	if (sentBytes == -1) RS_UNLIKELY co_return rFAILURE;
	totalSentBytes += sentBytes;
	RS_DBG4( pSocket, " sent dataTypeLen: ", static_cast<int>(dataTypeLen),
	         " sentBytes: ", sentBytes );

	sentBytes = co_await pSocket.send(
	            reinterpret_cast<const uint8_t*>(netMsg.mTypeName.data()),
	            dataTypeLen, errbub );
	if (sentBytes == -1) co_return rFAILURE;
	totalSentBytes += sentBytes;
	RS_DBG4( pSocket, " sent netMsg.mTypeName: ", netMsg.mTypeName,
	         " sentBytes: ", sentBytes);

	uint32_t dataTypeLenNetOrder = htonl(netMsg.mData.size());
	sentBytes = co_await pSocket.send(
	    reinterpret_cast<uint8_t*>(&dataTypeLenNetOrder), 4, errbub );
	if (sentBytes == -1) RS_UNLIKELY co_return rFAILURE;
	totalSentBytes += sentBytes;
	RS_DBG4( pSocket, " sent netMsg.mData.size(): ", netMsg.mData.size(),
	         " sentBytes: ", sentBytes );

	sentBytes = co_await pSocket.send(
	            reinterpret_cast<const uint8_t*>(netMsg.mData.data()),
	            netMsg.mData.size(), errbub );
	if (sentBytes == -1) RS_UNLIKELY co_return rFAILURE;
	totalSentBytes += sentBytes;

	/* Wait for total received bytes acknowledge, all tests without this worked
	 * fine anyway, so this has been added mainly to enable the sender to
	 * extimate sending time in user space */
	uint32_t totalAckBytes = 0;
	auto recvRet = co_await
	        pSocket.recv(
	            reinterpret_cast<uint8_t*>(&totalAckBytes), 4, errbub );
	if(recvRet == -1) RS_UNLIKELY co_return rFAILURE;

	const auto ackETP = steady_clock::now();

	totalAckBytes = htonl(totalAckBytes);
	if(totalAckBytes != totalSentBytes) RS_UNLIKELY
	{
		RS_WARN( "Peer acknowledged ", totalAckBytes,
		         " bytes instead of ", totalSentBytes );
		// TODO: pass peer address
		// TODO: define proper error condition instead of abusing std::errc
		rs_error_bubble_or_exit(
		            std::errc::message_size, errbub,
		            "Peer XXX acknowledge mismatch got: ",
		            totalAckBytes,
		            " expected: ", totalSentBytes );
		co_return rFAILURE;
	}

	/* RTT impact on bandwidth calculation should be usually neglegible, and for
	 * sure becomes even more neglegible when the network and hence the shared
	 * data size grows.
	 * Subtracting it causes negative result in some situations for no
	 * appreciable benefit in the rest of the cases so we don't take it in
	 * account here */
	if(ackETP > sendBTP) RS_LIKELY
	    netStats.mUpBwMbsExt = MbitPerSec(
	            totalSentBytes,
	            duration_cast<microseconds>(ackETP - sendBTP).count() );
	else
		RS_ERR( "Time must have wrapped during upload, bandwidth extimation ",
		        " ignored" );

	RS_DBG4( pSocket, " sent netMsg.mData: ", netMsg.mData);

	RS_DBG3( pSocket, " Total bytes sent: ", totalSentBytes );
	co_return totalSentBytes;
}

std::task<bool> SharedState::handleReqSyncConnection(
        std::shared_ptr<AsyncSocket> pSocket,
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
	RS_DBG3("IOContext status after clenup: ", ioContext); \
} \
while(false)

	NetworkStats netStats;
	pSocket->getPeerAddr(netStats.mPeer);

	if(!co_await SharedState::serverHandShake(
	            *pSocket, netStats, errbub )) RS_UNLIKELY
	{
		handleReqSyncConnection_clean_socket();
		co_return rFAILURE;
	}

	NetworkMessage networkMessage;
	std::error_condition recvErrc;
	auto totalReceived = co_await
	        receiveNetworkMessage(
	            *pSocket, networkMessage, netStats, &recvErrc );
	if(totalReceived < 0)
	{
		RS_DBG1("Got invalid data from client ", *pSocket);
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
	RS_DBG3("IOContext status after clenup: ", ioContext); \
} \
while(false)

	using namespace std::chrono;
	const auto mergeBTP = steady_clock::now();

	if( co_await
	        luaSharedState->writeStdIn(
	            networkMessage.mData.data(), networkMessage.mData.size(),
	            &tLSHErr ) == -1 ) RS_UNLIKELY
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

	const auto mergeETP = steady_clock::now();
	const auto mergeMuSecs = duration_cast<microseconds>(mergeETP - mergeBTP);

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

	auto totalSent = co_await sendNetworkMessage(
	            *pSocket, networkMessage, netStats, errbub );
	if(totalSent == -1)
	{
		handleReqSyncConnection_clean_socket_cmd();
		co_return rFAILURE;
	}

	sockaddr_storage peerAddr;
	pSocket->getPeerAddr(peerAddr);

	RS_INFO( "Handled sync request from peer: ", peerAddr,
	         " Received message type: ", networkMessage.mTypeName,
	         " Received message size: ", receivedMessageSize,
	         " Sent message size: ", networkMessage.mData.size(),
	         " Extimated upload BW: ", netStats.mUpBwMbsExt, "Mbit/s",
	         " Extimated download BW: ", netStats.mDownBwMbsExt, "Mbit/s",
	         " Extimated RTT: ", netStats.mRttExt.count(), "μs",
	         " Processing time: ", mergeMuSecs.count(), "μs"
	         " Total sent bytes: ", totalSent,
	         " Total received bytes: ", totalReceived );

	handleReqSyncConnection_clean_socket_cmd();

	co_return rSUCCESS && collectStat(netStats, errbub);
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

#if RS_DEBUG_LEVEL > 2
	RS_DBG( "Found ", peerAddresses.size(), " potential neighbours" );
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

/*static*/ std::task<bool> SharedState::serverHandShake(
        AsyncSocket& pSocket, NetworkStats& netStats, std::error_condition* errbub )
{
	uint32_t wireProtoVer = 0;
	auto recvRet = co_await pSocket.recv(
	        reinterpret_cast<uint8_t*>(&wireProtoVer), 4, errbub );
	if(recvRet == -1) RS_UNLIKELY co_return false;

	wireProtoVer = htonl(wireProtoVer);
	if(WIRE_PROTO_VERSION != wireProtoVer) RS_UNLIKELY
	{
		// TODO: pass peer address
		// TODO: define proper error condition instead of abusing std::errc
		rs_error_bubble_or_exit(
		            std::errc::protocol_error, errbub,
		            "Peer XXX wire protocol version mismatch got: ",
		            wireProtoVer, " expected: ", WIRE_PROTO_VERSION );
		co_return false;
	}

	/* Wire proto handshake is very very lightweight it seems acceptable to
	 * interact a bit longer to extimate RTT more precisely on both sides */

	using namespace std::chrono;
	const auto verBTP = steady_clock::now();
	wireProtoVer = htonl(WIRE_PROTO_VERSION);
	auto sendRet = co_await pSocket.send(
	            reinterpret_cast<const uint8_t*>(&wireProtoVer), 4, errbub );
	if(sendRet == -1) RS_UNLIKELY co_return false;

	recvRet = co_await pSocket.recv(
	        reinterpret_cast<uint8_t*>(&wireProtoVer), 4, errbub );
	if(recvRet == -1) RS_UNLIKELY co_return false;

	const auto verETP = steady_clock::now();
	netStats.mRttExt = duration_cast<microseconds>(verETP - verBTP);

	co_return true;
}

/*static*/ std::task<bool> SharedState::clientHandShake(
        AsyncSocket& pSocket, NetworkStats& netStats, std::error_condition* errbub )
{
	/* Wire proto handshake seems an acceptable interacion to extimate RTT on
	 * both sides */

	using namespace std::chrono;
	const auto verBTP = steady_clock::now();

	uint32_t wireProtoVer = htonl(WIRE_PROTO_VERSION);

	auto sendRet = co_await pSocket.send(
	            reinterpret_cast<const uint8_t*>(&wireProtoVer), 4, errbub );
	if(sendRet == -1) RS_UNLIKELY co_return false;

	auto recvRet = co_await pSocket.recv(
	        reinterpret_cast<uint8_t*>(&wireProtoVer), 4, errbub );
	if(recvRet == -1) RS_UNLIKELY co_return false;

	const auto verETP = steady_clock::now();

	wireProtoVer = htonl(wireProtoVer);
	if(WIRE_PROTO_VERSION != wireProtoVer) RS_UNLIKELY
	{
		// TODO: pass peer address
		// TODO: define proper error condition instead of abusing std::errc
		rs_error_bubble_or_exit(
		            std::errc::protocol_error, errbub,
		            "Peer XXX wire protocol version mismatch got: ",
		            wireProtoVer, " expected: ", WIRE_PROTO_VERSION );
		co_return false;
	}

	sendRet = co_await pSocket.send(
	            reinterpret_cast<const uint8_t*>(&wireProtoVer), 4, errbub );
	if(sendRet == -1) RS_UNLIKELY co_return false;

	netStats.mRttExt = duration_cast<microseconds>(verETP - verBTP);

	co_return true;
}

/*static*/ bool SharedState::collectStat(
        NetworkStats& netStat,
        std::error_condition* errbub )
{
	const std::string statPath(SHARED_STATE_NET_STAT_FILE_PATH);
	const std::string tPeerStr = sockaddr_storage_iptostring(netStat.mPeer);
	const auto tNow = std::chrono::steady_clock::now();

#ifdef SHARED_STATE_STAT_FILE_LOCKING
	int openRet = open(
	            statPath.c_str(),
	            O_RDWR | O_CREAT | O_CLOEXEC,
	            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if(openRet == -1) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "Failure opening network statistics file: ", statPath );
		return false;
	}

	int flockRet = flock(openRet, LOCK_EX);
	if(flockRet == -1) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "Failure acquiring lock on network statistics file: ",
		            statPath );
		return false;
	}
#endif // def SHARED_STATE_STAT_FILE_LOCKING

	json newJsonStats = json::object();

	{
		std::ifstream statsFileReadStream(statPath);
		if(statsFileReadStream.is_open())
		{
			json jsonStats = json::parse(statsFileReadStream, nullptr, false);
			if(!jsonStats.is_discarded() && jsonStats.is_object())
			{
				for(auto& [ePeer, eRecords] : std::as_const(jsonStats).items())
				{
					int mSkip = 0;
					if(ePeer == tPeerStr)
					{
						mSkip = std::max<int>(
						    eRecords.size() - SHARED_STATE_NET_STAT_MAX_RECORDS,
						    0 );
						RS_DBG1("Skip: ", mSkip, " records to keep size at bay");
					}

					for( auto mIt = eRecords.begin() + mSkip;
					     mIt != eRecords.end();  ++mIt )
					{
						std::chrono::time_point<std::chrono::steady_clock> tTS{};

						if( mIt->contains("mTS") &&
						        mIt->at("mTS").is_number_integer() )
							tTS = std::chrono::time_point<std::chrono::steady_clock>(
							            std::chrono::time_point<std::chrono::steady_clock>::duration(
							            mIt->at("mTS").get<int64_t>() ) );

						auto tAge = std::chrono::duration_cast<std::chrono::minutes>(
						            tNow - tTS );
						if( tAge > SHARED_STATE_NET_STAT_MAX_AGE )
						{
							RS_DBG3( "Skipping too old record with age: ", tAge);
						}
						else
						{
							RS_DBG3( "Keeping record with age: ", tAge );
							newJsonStats[ePeer].push_back(*mIt);
						}
					}
				}
			}
			else RS_WARN("Discarding corrupted or empty stored statistics");
		}
		statsFileReadStream.close();
	}

	netStat.mTS = tNow;
	newJsonStats[tPeerStr].push_back(netStat);

	{
		std::ofstream statsFileWriteStream(
		            statPath, std::ios::out | std::ios::trunc );
		if(!statsFileWriteStream.is_open())
		{
			RS_ERR("Failure opening network statistic files in write mode");
			rs_error_bubble_or_exit(
			            std::errc::io_error, errbub,
			            "Failure opening network statistic files in write mode" );
			return false;
		}

		statsFileWriteStream << newJsonStats;
		statsFileWriteStream.close();
	}

#ifdef SHARED_STATE_STAT_FILE_LOCKING
	flockRet = flock(openRet, LOCK_UN);
	if(flockRet == -1) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "Failure releasing lock on network statistics file: ",
		            statPath );
		return false;
	}
	if(close(openRet) == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "Failure closing network statistics file: ",
		            statPath );
		return false;
	}
#endif // def SHARED_STATE_STAT_FILE_LOCKING

	return true;
}
