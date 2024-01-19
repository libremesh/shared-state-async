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
#include <fstream>

#include <util/rsjson.h>
#include <serialiser/rsserializable.h>

#include "task.hh"
#include "async_socket.hh"

struct SharedState
{
	explicit SharedState(IOContext& ioContext): mIoContext(ioContext) {}

	static constexpr uint16_t TCP_PORT = 3490;

	static constexpr uint16_t DATA_TYPE_NAME_MAX_LENGHT = 128;

	/** TODO: This is being used around the code both for "distilled" data size
	 * and full state which have a noticeable overhead so is bigger, a different
	 * costant should be used */
	static constexpr uint32_t DATA_MAX_LENGHT = 1024*1024; // 1MB

	static constexpr std::string_view SHARED_STATE_CONFIG_DIR =
	        "/tmp/shared-state/";

	static constexpr std::string_view SHARED_STATE_HOOKS_DIR =
	        "/usr/share/shared-state/hooks/";

	static constexpr std::string_view SHARED_STATE_CONFIG_FILE_NAME =
	        "shared-state-async.conf";

	struct DataTypeConf : RsSerializable
	{
		std::string mName;

		std::string mScope;

		std::chrono::seconds mUpdateInterval;

		std::chrono::seconds mBleachTTL;

		/// @see RsSerializable
		virtual void
		serial_process(RsGenericSerializer::SerializeJob j,
		               RsGenericSerializer::SerializeContext &ctx);

		/*protected:
		 *	friend RsTypeSerializer;
		 * TODO: Should be used only by RsTypeSerializer, but apparently friend
		 * declaration wasn't enough to make them visible to it */
		DataTypeConf(): mName(), mScope(),
		    mUpdateInterval(std::chrono::seconds::zero()),
		    mBleachTTL(std::chrono::seconds::zero()) {}
		DataTypeConf(const DataTypeConf& st):
		    mName(st.mName), mScope(st.mScope),
		    mUpdateInterval(st.mUpdateInterval), mBleachTTL(st.mBleachTTL)
		{}
	};

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
	struct NetworkStats : RsSerializable
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

		/// @see RsSerializable
		virtual void
		serial_process(RsGenericSerializer::SerializeJob j,
		               RsGenericSerializer::SerializeContext &ctx);
	};

	using StateKey = std::string;

	struct StateEntry : RsSerializable
	{
		StateEntry(
		        const std::string& author,
		        uint64_t bleachTTL,
		        const RsJson& pData = RsJson(),
		        RsJson::AllocatorType* allocator = nullptr ):
		    mAuthor(author), mBleachTTL(bleachTTL), mData(allocator)
		{ mData.CopyFrom(pData, allocator ? *allocator : mData.GetAllocator()); }

		std::string mAuthor;
		uint64_t mBleachTTL;
		RsJson mData;

		/// @see RsSerializable
		virtual void
		serial_process( RsGenericSerializer::SerializeJob j,
		                RsGenericSerializer::SerializeContext &ctx );

	/*protected:
	 *	friend RsTypeSerializer;
	 * TODO: Should be used only by RsTypeSerializer, but apparently firend
	 * declaration wasn't enough to make them visible to it */
		StateEntry(): mAuthor(), mBleachTTL(0), mData() {}
		StateEntry(const StateEntry& st):
		    mAuthor(st.mAuthor), mBleachTTL(st.mBleachTTL)
		{
			/* Reusing same allocator prooved unsafe, in particular passing
			 * const_cast<StateEntry&>(st).mData.GetAllocator()
			 * caused memory corruption in rapidjson and subsequently a crash */
			mData.CopyFrom(st.mData, mData.GetAllocator());
		}
	};

	/** @return number of significative changes in the state, -1 on error */
	ssize_t bleach(
	    const std::string& dataTypeName,
	    std::error_condition* errbub = nullptr );

	/** @return number of significative changes in the state, -1 on error */
	std::task<ssize_t> merge(
	    const std::string& dataTypeName,
	    const std::map<StateKey, StateEntry>& stateSlice,
	    std::error_condition* errbub = nullptr );

	/**
	 * @return returns false if error occurred, true otherwise
	 */
	std::task<bool> notifyHooks(
	        const std::string& typeName, std::error_condition* errbub = nullptr );

	/**
	 * @return returns false if error occurred, true otherwise
	 */
	std::task<bool> syncWithPeer(
	        std::string dataTypeName, const sockaddr_storage& peerAddr,
	        std::error_condition* errbub = nullptr );

	/**
	 * @return returns false if error occurred, true otherwise
	 */
	std::task<bool> handleReqSyncConnection(
	        std::shared_ptr<AsyncSocket> clientSocket,
	        std::error_condition* errbub = nullptr );

	/**
	 * @return returns false if error occurred, true otherwise
	 */
	static std::task<bool> getCandidatesNeighbours(
	        std::vector<sockaddr_storage>& peerAddresses,
	        IOContext& ioContext, std::error_condition* errbub = nullptr );

	bool loadRegisteredTypes(
	        std::error_condition* errbub = nullptr );

	bool registerDataType(
	        const std::string& typeName, const std::string& typeScope,
	        std::chrono::seconds updateInterval, std::chrono::seconds ttl,
	        std::error_condition* errbub = nullptr );


	/**** TEMPORARY STUFF */
	static const sockaddr_storage& localInstanceAddr();

private:
	static constexpr std::string_view SHARED_STATE_GET_CANDIDATES_CMD =
	        "shared-state-async-discover";

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

		void fromStateSlice(std::map<StateKey, StateEntry>& stateSlice);
		void toStateSlice(std::map<StateKey, StateEntry>& stateSLice) const;
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

	static constexpr std::string_view SHARED_STATE_DATA_DIR =
	        "/tmp/shared-state/";

	static constexpr std::string_view SHARED_STATE_NET_STAT_FILE_PATH =
	        "/tmp/shared-state/network_statistics.json";

	static constexpr uint8_t SHARED_STATE_NET_STAT_MAX_RECORDS = 10;

	static constexpr std::chrono::minutes SHARED_STATE_NET_STAT_MAX_AGE =
	        std::chrono::minutes(30);

	static bool collectStat(
	        NetworkStats& netStats,
	        std::error_condition* errbub = nullptr );

protected:
	RS_DEPRECATED
	static std::string authorPlaceOlder()
	{
		std::ifstream hostReadStream("/proc/sys/kernel/hostname");
		std::string hostName;
		hostReadStream >> hostName;
		return hostName;
	}

	/// Shared state in memory storage
	std::map<std::string, std::map<StateKey, StateEntry>> mStates;

	/// Shared state data types loaded configurations
	std::map<std::string, DataTypeConf> mTypeConf;

	IOContext& mIoContext;
};
