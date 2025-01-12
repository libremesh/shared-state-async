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
#include <filesystem>
#include <deque>

#ifdef SHARED_STATE_STAT_FILE_LOCKING
#	include <fcntl.h>
#	include <sys/file.h>
#endif // def SHARED_STATE_STAT_FILE_LOCKING

#include <rapidjson/istreamwrapper.h>

#include <util/rsnet.h>
#include <util/rsurl.h>
#include <serialiser/rstypeserializer.h>

#include "sharedstate.hh"
#include "async_socket.hh"
#include "async_command.hh"
#include "shared_state_errors.hh"

#include <util/rsdebug.h>
#include <util/rserrorbubbleorexit.h>
#include <util/rsdebuglevel2.h>


std::task<bool> SharedState::syncWithPeer(
        std::string dataTypeName, const sockaddr_storage& peerAddr,
        std::error_condition* errbub )
{
	RS_DBG3(dataTypeName, " ", sockaddr_storage_tostring(peerAddr), " ", errbub);

	constexpr bool rFAILURE = false;
	constexpr bool rSUCCESS = true;

	const auto statesIt = mStates.find(dataTypeName);
	if(statesIt == mStates.end())
	{
		rs_error_bubble_or_exit( SharedStateErrors::UNKOWN_DATA_TYPE, errbub,
		                         dataTypeName );
		co_return rFAILURE;
	}
	auto& tState = statesIt->second;

	auto tSocket = co_await ConnectingSocket::connect(
	            peerAddr, mIoContext, errbub);
	if(!tSocket) co_return rFAILURE;

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
	co_await mIoContext.closeAFD(tSocket); \
	RS_DBG3("IOContext status after clenup: ", mIoContext); \
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

	SharedState::NetworkMessage netMessage;
	netMessage.mTypeName = dataTypeName;
	netMessage.fromStateSlice(tState);

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

	std::map<StateKey, StateEntry> receivedState;
	netMessage.toStateSlice(receivedState);

	ssize_t changes = co_await merge(
	            netMessage.mTypeName, receivedState, peerAddr, errbub);
	if(changes == -1)
	{
		syncWithPeer_clean_socket();
		co_return rFAILURE;
	}
	const auto mergeETP = steady_clock::now();
	const auto mergeMuSecs = duration_cast<microseconds>(mergeETP - mergeBTP);

	syncWithPeer_clean_socket();

	RS_DBG3( "Synchronized with peer: ", peerAddr,
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

	if(!collectStat(netStats, errbub)) co_return rFAILURE;

	if(isPeer && (changes > 0))
		co_return co_await notifyHooks(netMessage.mTypeName, errbub);

	co_return rSUCCESS;
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
	co_await mIoContext.closeAFD(pSocket); \
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

	using namespace std::chrono;
	const auto mergeBTP = steady_clock::now();

	std::map<StateKey, StateEntry> peerState;
	networkMessage.toStateSlice(peerState);

	ssize_t changes = co_await merge(
	            networkMessage.mTypeName, peerState, netStats.mPeer, errbub );

	if(changes == -1) RS_UNLIKELY
	{
		handleReqSyncConnection_clean_socket();
		co_return rFAILURE;
	}

	const auto mergeETP = steady_clock::now();
	const auto mergeMuSecs = duration_cast<microseconds>(mergeETP - mergeBTP);

	networkMessage.fromStateSlice(mStates[networkMessage.mTypeName]);

	auto totalSent = co_await sendNetworkMessage(
	            *pSocket, networkMessage, netStats, errbub );
	if(totalSent == -1)
	{
		handleReqSyncConnection_clean_socket();
		co_return rFAILURE;
	}

	RS_DBG3( "Handled sync request from peer: ", netStats.mPeer,
	         " Received message type: ", networkMessage.mTypeName,
	         " Received message size: ", receivedMessageSize,
	         " Sent message size: ", networkMessage.mData.size(),
	         " Extimated upload BW: ", netStats.mUpBwMbsExt, "Mbit/s",
	         " Extimated download BW: ", netStats.mDownBwMbsExt, "Mbit/s",
	         " Extimated RTT: ", netStats.mRttExt.count(), "μs",
	         " Processing time: ", mergeMuSecs.count(), "μs"
	         " Total sent bytes: ", totalSent,
	         " Total received bytes: ", totalReceived );

	handleReqSyncConnection_clean_socket();

	if(!collectStat(netStats, errbub)) co_return rFAILURE;

	if(isPeer && (changes > 0))
		co_return co_await notifyHooks(networkMessage.mTypeName, errbub);

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

#if RS_DEBUG_LEVEL > 2
	RS_DBG( "Found ", peerAddresses.size(), " potential neighbours" );
	for( auto&& peerAddr : std::as_const(peerAddresses))
		RS_DBG(sockaddr_storage_iptostring(peerAddr));
#endif // RS_DEBUG_LEVEL

	co_return true;
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
	sockaddr_storage_ipv4_to_ipv6(netStat.mPeer);
	const std::string tPeerStr = sockaddr_storage_iptostring(netStat.mPeer);
	const auto tNow = std::chrono::steady_clock::now();
	netStat.mTS = tNow;

	RS_DBG3(tPeerStr);

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

	std::map<std::string, std::deque<NetworkStats>> stats;

	std::ifstream statFileReadStream(statPath);
	if(statFileReadStream.is_open())
	{
		rapidjson::IStreamWrapper jStream(statFileReadStream);
		RsGenericSerializer::SerializeJob j(RsGenericSerializer::FROM_JSON);
		RsGenericSerializer::SerializeContext ctx;
		ctx.mJson.ParseStream(jStream);
		statFileReadStream.close();
		RS_SERIAL_PROCESS(stats);
	}
	else RS_WARN("Discarding corrupted or empty statistics file: ", statPath);


	stats[tPeerStr].push_back(netStat);

	// Prune excessive or too old records
	for(auto&& [peerStr, peerStats] : stats)
	{
		int mSkip = std::max<int>(
		                peerStats.size() - SHARED_STATE_NET_STAT_MAX_RECORDS, 0 );
		RS_DBG4( "Skip: ", mSkip,
		         " records for peer: ", peerStr," to keep size at bay" );
		while(mSkip-- > 0) peerStats.pop_front();

		while( !peerStats.empty() &&
		       ((tNow - peerStats.front().mTS) > SHARED_STATE_NET_STAT_MAX_AGE) )
			peerStats.pop_front();
	}

	std::ofstream statsFileWriteStream(
	            statPath, std::ios::out | std::ios::trunc );
	if(statsFileWriteStream.is_open())
	{
		RsGenericSerializer::SerializeJob j(RsGenericSerializer::TO_JSON);
		RsGenericSerializer::SerializeContext ctx;
		RS_SERIAL_PROCESS(stats);
		if(ctx.mOk) statsFileWriteStream << ctx.mJson << std::endl;
		statsFileWriteStream.close();
	}
	else rs_error_bubble_or_exit(std::errc::io_error, errbub, statPath);

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

bool SharedState::registerDataType(
        const std::string& typeName, const std::string& typeScope,
        std::chrono::seconds updateInterval, std::chrono::seconds TTL,
        std::error_condition* errbub )
{
	if(typeName.empty())
	{
		rs_error_bubble_or_exit(
		            std::errc::invalid_argument, errbub,
		            "empty type name" );
		return false;
	}

	if(typeName.size() > DATA_TYPE_NAME_MAX_LENGHT)
	{
		rs_error_bubble_or_exit(
		            std::errc::invalid_argument, errbub,
		            "type name too long" );
		return false;
	}


	const std::string tConfigPath(
	            std::string(SHARED_STATE_CONFIG_DIR) +
	            std::string(SHARED_STATE_CONFIG_FILE_NAME) );

	std::error_condition loadErr;
	if(!loadRegisteredTypes(&loadErr))
	{
		RS_INFO( "Config file: ", tConfigPath,
		         " corrupted or non-existent, creating a new one" );

		namespace fs = std::filesystem;
		const fs::path confPath(SHARED_STATE_CONFIG_DIR);
		std::error_code confDirErr;
		fs::create_directory(confPath, confDirErr);
		if(confDirErr)
		{
			rs_error_bubble_or_exit(
			            confDirErr.default_error_condition(), errbub,
			            "Failure creating config directory" );
			return false;
		}
	}

	auto&& tConf = mTypeConf[typeName];
	tConf.mName = typeName;
	tConf.mBleachTTL = TTL;
	tConf.mScope = typeScope;
	tConf.mUpdateInterval = updateInterval;
	// TODO: use inizializer list costructor

	RsGenericSerializer::SerializeJob j(RsGenericSerializer::TO_JSON);
	RsGenericSerializer::SerializeContext ctx;
	RS_SERIAL_PROCESS(mTypeConf);

	std::ofstream confFileWriteStream(
	            tConfigPath, std::ios::out | std::ios::trunc );
	if(!confFileWriteStream.is_open())
	{
		RS_ERR("Failure opening config file:", tConfigPath, " for writing");
		rs_error_bubble_or_exit(std::errc::io_error, errbub);
		return false;
	}
	confFileWriteStream << ctx.mJson << std::endl;

	return ctx.mOk;
}


bool SharedState::loadRegisteredTypes(std::error_condition* errbub)
{
	const std::string tConfigPath(
	            std::string(SHARED_STATE_CONFIG_DIR) +
	            std::string(SHARED_STATE_CONFIG_FILE_NAME) );

	std::ifstream confFileReadStream(tConfigPath);
	if(!confFileReadStream.is_open())
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "Failure opening config file for reading: ", tConfigPath );
		return false;
	}
	rapidjson::IStreamWrapper jStream(confFileReadStream);

	RsGenericSerializer::SerializeJob j(RsGenericSerializer::FROM_JSON);
	RsGenericSerializer::SerializeContext ctx;
	ctx.mJson.ParseStream(jStream);

	if(ctx.mJson.HasParseError())
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "Corrupted type config file:", tConfigPath );
		return false;
	}

	/* RsTypeSerializer print an error message and then clear the map if it is
	 * not empty before FROM_JSON, so let's do it and avoid the error message
	 * @see https://github.com/libremesh/lime-packages/issues/1081 */
	mTypeConf.clear();
	RS_SERIAL_PROCESS(mTypeConf);

	// Create empty states for new types, do nothing if already presents
	for(auto&& [typeName, _]: mTypeConf) mStates[typeName];

	// Remove states for types that are not registered anymore
	for(auto sIt = mStates.begin(); sIt != mStates.end();)
		if(mTypeConf.find(sIt->first) == mTypeConf.end())
			sIt = mStates.erase(sIt);
		else ++sIt;

	return ctx.mOk;
}

void SharedState::StateEntry::serial_process(
        RsGenericSerializer::SerializeJob j,
        RsGenericSerializer::SerializeContext& ctx)
{
	RS_SERIAL_PROCESS(mAuthor);

	decltype(mTtl)::rep tTtl = mTtl.count();
	RsTypeSerializer::serial_process(j, ctx, tTtl, "mTtl");
	mTtl = decltype(mTtl)(tTtl);

	RS_SERIAL_PROCESS(mData);
}


/*static*/ const sockaddr_storage& SharedState::localInstanceAddr()
{
	static sockaddr_storage lAddr{};
	static bool mInitialized = false;
	if(!mInitialized)
	{
		sockaddr_storage_ipv4_aton(lAddr, "127.0.0.1");
		sockaddr_storage_setport(lAddr, TCP_PORT);
		mInitialized = true;
	}

	return lAddr;
}

std::task<ssize_t> SharedState::merge(
        const std::string& dataTypeName,
        const std::map<StateKey, StateEntry>& stateSlice,
        const sockaddr_storage& peerAddr,
        std::error_condition* errbub )
{
	constexpr ssize_t rFAILURE = -1;

	RS_DBG3(dataTypeName, " slice size: ", stateSlice.size());

	const auto statesIt = mStates.find(dataTypeName);
	if(statesIt == mStates.end())
	{
		rs_error_bubble_or_exit( SharedStateErrors::UNKOWN_DATA_TYPE, errbub,
		                         dataTypeName );
		co_return rFAILURE;
	}

	const auto typeUpdateInterval = mTypeConf.at(dataTypeName).mUpdateInterval;

	auto& tState = statesIt->second;
	ssize_t allChanges = 0;
	ssize_t significantChanges = 0;
	const bool isRemote = !sockaddr_storage_isLoopbackNet(peerAddr);

	for(auto&& [stateKey, sliceEntry]: stateSlice)
	{
		const auto knownEntryIt = tState.find(stateKey);
		if(knownEntryIt == tState.end()) RS_UNLIKELY
		{
			tState.emplace(stateKey, sliceEntry);
			++significantChanges; ++allChanges;
			RS_DBG4("Inserted new entry with key: ", stateKey);
			continue;
		}
		const auto& knownEntry = knownEntryIt->second;
		const bool ownAuthorship = knownEntry.mAuthor == authorPlaceOlder();

		/* When receiving data authored by this node from remote nodes with
		 * higher TTL then our own, something fishy is happening.
		 * Ignore the remote entry and print a warning.
		 * Instead when the data is received from the CLI if some own authored
		 * record have higher TTL its normal and overwriting is accepted */
		if( isRemote && ownAuthorship &&
		        sliceEntry.mTtl > knownEntry.mTtl ) RS_UNLIKELY
		{
			RS_WARN( "Discarding received known entry: ", stateKey, " ",
			         "authored by this node with higher TTL from remote peer: ",
			         peerAddr, " is remote peer ill?" );
			continue;
		}

		/* Avoid overwrite of own entries from remotes peers when the TTL is
		 * equal, while keep accepting it from CLI */
		const auto minUpdateTtl = (isRemote && ownAuthorship) ?
		            knownEntry.mTtl + std::chrono::seconds(1) : knownEntry.mTtl;

		if( sliceEntry.mTtl >= knownEntry.mTtl )
		{
			bool significant = knownEntry.mData != sliceEntry.mData;
			RS_DBG4( "Updating entry with key: ", stateKey, " TTL: ",
			         sliceEntry.mTtl, " > ", knownEntry.mTtl,
			         " significant: ", significant? "true" : "false" );
			if(significant) ++significantChanges;
			++allChanges;
			tState.erase(stateKey);
			tState.emplace(stateKey, sliceEntry);
		}
	}

#if RS_DEBUG_LEVEL > 1
	RS_DBG( dataTypeName, " got ", significantChanges,
	        " significative changes out of ", allChanges,
	        " input slice size: ", stateSlice.size(),
	        " state size: ", tState.size() );
#endif
	co_return significantChanges;
}


void SharedState::NetworkMessage::fromStateSlice(
        std::map<StateKey, StateEntry>& stateSlice )
{
	/* !!Keep stateSlice paramather name the same as in toStateSlice */

	RsGenericSerializer::SerializeJob j(RsGenericSerializer::TO_JSON);
	RsGenericSerializer::SerializeContext ctx;
	RS_SERIAL_PROCESS(stateSlice);

	std::stringstream ss;
	ss << ctx.mJson;
	mData.assign(ss.view().begin(), ss.view().end());
}

void SharedState::NetworkMessage::toStateSlice(
        std::map<StateKey, StateEntry>& stateSlice ) const
{
	/* !! Keep stateSlice paramather name the same as in fromStateSlice */

	RsGenericSerializer::SerializeJob j(RsGenericSerializer::FROM_JSON);
	RsGenericSerializer::SerializeContext ctx;
	ctx.mJson.Parse(
	            reinterpret_cast<const char*>(mData.data()),
	            mData.size() );
	RS_DBG4(ctx.mJson);
	RS_SERIAL_PROCESS(stateSlice);
}

std::task<bool> SharedState::notifyHooks(
        const std::string& typeName, std::error_condition* errbub )
{
	RS_DBG2(typeName);

	auto statesIt = mStates.find(typeName);
	if(statesIt == mStates.end())
	{
		rs_error_bubble_or_exit( SharedStateErrors::UNKOWN_DATA_TYPE, errbub,
		                         typeName );
		co_return false;
	}
	const auto& tState = statesIt->second;

	const std::string hooksDirStr = std::string(SHARED_STATE_HOOKS_DIR) +
	        typeName + "/";

	namespace fs = std::filesystem;

	std::error_code fsErr;
	if(!fs::is_directory(hooksDirStr, fsErr))
		co_return false; // No hooks, nothing to do

	std::string tDataStr;

	{
		RsJson cleanJsonData(rapidjson::kObjectType);
		auto& jAllocator = cleanJsonData.GetAllocator();

		for(const auto& [key, stateEntry]: tState)
		{
			rapidjson::Value jKey(
			            key.c_str(),
			            static_cast<rapidjson::SizeType>(key.length()),
			            jAllocator );

			rapidjson::Value jValue(stateEntry.mData, jAllocator);

			cleanJsonData.AddMember(jKey, jValue, jAllocator);
		}

		std::stringstream ss;
		ss << cleanJsonData;
		tDataStr = ss.str();
	}

	for (auto const& dirEntry : fs::directory_iterator{hooksDirStr})
	{
		auto&& hookPath = dirEntry.path();
		auto&& stRet = dirEntry.status(fsErr);
		if(fsErr)
		{
			RS_ERR( "Skipping invalid hook: ", hookPath, " ",
			         fsErr.default_error_condition() );
			continue;
		}

		if(fs::perms::none == (fs::perms::owner_exec & stRet.permissions()))
		{
			RS_ERR( "Skipping non-executable hook: ", hookPath, " ",
			         fsErr.default_error_condition() );
			continue;
		}

		std::error_condition hookErr;
		auto hookCmd = AsyncCommand::execute(
		            dirEntry.path(), mIoContext, &hookErr );
		if(!hookCmd)
		{
			RS_ERR("Failure executing hook: ", hookPath, " ", hookErr);
			continue;
		}

		co_await hookCmd->writeStdIn(
		    reinterpret_cast<uint8_t*>(tDataStr.data()), tDataStr.size(),
		            &hookErr );
		co_await hookCmd->closeStdIn(&hookErr);
		co_await AsyncCommand::waitTermination(hookCmd, &hookErr);

		if(hookErr)
			RS_ERR("Hook: ", hookPath, " failed with: ", hookErr);
#if RS_DEBUG_LEVEL > 1
		else
			RS_DBG("Success executing hook: ", hookPath);
#endif // RS_DEBUG_LEVEL
	}

	co_return true;
}

ssize_t SharedState::bleach(
        const std::string& dataTypeName, std::chrono::seconds times,
        std::error_condition* errbub )
{
	if(times < std::chrono::seconds(1)) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
		            std::errc::invalid_argument, errbub,
		            dataTypeName );
		return -1;
	}

	auto statesIt = mStates.find(dataTypeName);
	if(statesIt == mStates.end()) RS_UNLIKELY
	{
		rs_error_bubble_or_exit(
		            SharedStateErrors::UNKOWN_DATA_TYPE, errbub,
		            dataTypeName );
		return -1;
	}
	auto& tState = statesIt->second;

	ssize_t significativeChanges =
	        std::erase_if(tState, [=](const auto& item)
	{ return item.second.mTtl <= times; });

	for(auto& [key, stateEntry]: tState) stateEntry.mTtl -= times;

	return significativeChanges;
}

void SharedState::DataTypeConf::serial_process(
        RsGenericSerializer::SerializeJob j,
        RsGenericSerializer::SerializeContext &ctx)
{
	RS_SERIAL_PROCESS(mName);
	RS_SERIAL_PROCESS(mScope);

	decltype(mUpdateInterval)::rep tUpdateInterval = mUpdateInterval.count();
	RsTypeSerializer::serial_process(j, ctx, tUpdateInterval, "mUpdateInterval");
	mUpdateInterval = decltype(mUpdateInterval)(tUpdateInterval);

	decltype(mBleachTTL)::rep tBleachTTL = mBleachTTL.count();
	RsTypeSerializer::serial_process(j, ctx, tBleachTTL, "mBleachTTL");
	mBleachTTL = decltype(mBleachTTL)(tBleachTTL);
}

void SharedState::NetworkStats::serial_process(
        RsGenericSerializer::SerializeJob j,
        RsGenericSerializer::SerializeContext &ctx)
{
	/* Do not serialize peer address as it ends up being reduntant with
	 * statistic storage key which is peer address */
	//std::string tPeer = sockaddr_storage_iptostring(mPeer);
	//RsTypeSerializer::serial_process(j, ctx, tPeer, "mPeer");
	//sockaddr_storage_inet_pton(mPeer, tPeer);

	using tp_t = decltype(mTS);
	using dur_t = tp_t::duration;
	int64_t tmTS = mTS.time_since_epoch().count();
	RsTypeSerializer::serial_process(j, ctx, tmTS, "mTS");
	mTS = tp_t(dur_t(tmTS));

	decltype(mRttExt)::rep tRTT = mRttExt.count();
	RsTypeSerializer::serial_process(j, ctx, tRTT, "mRttExt");
	mRttExt = decltype(mRttExt)(tRTT);

	RS_SERIAL_PROCESS(mUpBwMbsExt);
	RS_SERIAL_PROCESS(mDownBwMbsExt);
}
