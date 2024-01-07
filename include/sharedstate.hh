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

#pragma once

#include <iostream>
#include <system_error>
#include <cstdint>
#include <vector>
#include <chrono>

#include "task.hh"
#include "async_socket.hh"


struct SharedState
{
	static constexpr uint16_t TCP_PORT = 3490;
	static constexpr uint16_t DATA_TYPE_NAME_MAX_LENGHT = 128;
	static constexpr uint32_t DATA_MAX_LENGHT = 1024*1024; // 1MB

	/**
	 * @return returns false if error occurred, true otherwise
	 */
	static std::task<bool> syncWithPeer(
	        std::string dataTypeName, const sockaddr_storage& peerAddr,
	        IOContext& ioContext, std::error_condition* errbub = nullptr );

	/**
	 * @return returns false if error occurred, true otherwise
	 */
	static std::task<bool> handleReqSyncConnection(
	        std::shared_ptr<AsyncSocket> clientSocket,
	        std::error_condition* errbub = nullptr );


	/**
	 * @return returns false if error occurred, true otherwise
	 */
	static std::task<bool> getCandidatesNeighbours(
	        std::vector<sockaddr_storage>& peerAddresses,
	        IOContext& ioContext, std::error_condition* errbub = nullptr );

	/**
	 * @return returns false if error occurred, true otherwise
	 */
	static std::task<bool> getState(
		const std::string& dataType,
		std::vector<uint8_t>& dataStorage,
		IOContext& ioContext,
		std::error_condition* errbub = nullptr );


	static std::task<bool> mergeSlice(
		const std::string& dataTypeName,
		const std::vector<uint8_t>& dataSlice,
		IOContext& ioContext,
		std::error_condition* errbub = nullptr );

	/** Measuring socket performances is a tricky businnes due to complete lack
	 * of statistic support from standard socket API plus kernel buffering
	 * magic, so each field of this struct is extimated in the best place we
	 * can, and a reference to the struct is passed around. Also the protocol
	 * have some modification just to make possible to have reasonable
	 * measurements/extimations.
	 * This way we obtain good enough bandwidht and round trip time statistics
	 * almost passively, the more data is shared the more accurated the
	 * extimation will be, this is expecially important because the more crowded
	 * is the network the more important becomes to make optimal routing
	 * decisions based on available BW.
	 */
	struct NetworkStats
	{
		NetworkStats(): mPeer(), mRttExt(0), mUpBwMbsExt(0), mDownBwMbsExt(0) {}

		sockaddr_storage mPeer;

		/** Statistic record collection timestamp */
		std::chrono::time_point<std::chrono::steady_clock> mTS;

		/** Round trip time extimation */
		std::chrono::microseconds mRttExt;

		/** Upload bandwidth extimation in Mbit/s */
		uint32_t mUpBwMbsExt;

		/** Download bandwidth extimation in Mbit/s */
		uint32_t mDownBwMbsExt;
	};

private:
	static constexpr std::string_view SHARED_STATE_LUA_CMD =
			"shared-state";

	static constexpr std::string_view SHARED_STATE_GET_CANDIDATES_CMD =
			"shared-state-get_candidates_neigh";

	static constexpr uint32_t WIRE_PROTO_VERSION = 1;

	static inline constexpr auto MbitPerSec(auto bytes, auto microseconds)
	{
		/* Both dividend and divisor have 10^6 scaling so no need to scale both.
		 * If possible keep using integer math which is faster.
		 * Left shift 3 is equivalent to multiply for 8 = 2^3 but much faster */
		return std::max<decltype(bytes)>(1, (bytes<<3)/microseconds);
	}

	/** The message format on the wire is:
	* |     1 byte       |           |   4 bytes   |      |
	* | type name lenght | type name | data lenght | data |
	*/
	struct NetworkMessage
	{
		std::string mTypeName;
		std::vector<uint8_t> mData;
	};

	static std::task<bool> clientHandShake(
	        AsyncSocket& pSocket, NetworkStats& netStats,
	        std::error_condition* errbub = nullptr );

	static std::task<bool> serverHandShake(
	        AsyncSocket& pSocket, NetworkStats& netStats,
	        std::error_condition* errbub = nullptr );

	static std::task<ssize_t> receiveNetworkMessage(
	        AsyncSocket& socket, NetworkMessage& netMsg,
	        NetworkStats& netStats,
	        std::error_condition* errbub = nullptr );

	static std::task<ssize_t> sendNetworkMessage(
	        AsyncSocket& socket, const NetworkMessage& netMsg,
	        NetworkStats& netStats,
	        std::error_condition* errbub = nullptr );

	static constexpr std::string_view SHARED_STATE_NET_STAT_FILE_PATH =
	        "/tmp/shared-state-network_statistics.json";

	static constexpr uint8_t SHARED_STATE_NET_STAT_MAX_RECORDS = 10;

	static constexpr std::chrono::minutes SHARED_STATE_NET_STAT_MAX_AGE =
	        std::chrono::minutes(30);

	static bool collectStat(
	        NetworkStats& netStats,
	        std::error_condition* errbub = nullptr );
};
