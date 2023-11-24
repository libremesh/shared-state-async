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

#pragma once

#include <iostream>
#include <system_error>
#include <cstdint>
#include <vector>

#include "task.hh"
#include "socket.hh"


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
	        std::shared_ptr<Socket> clientSocket,
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

private:
	static constexpr std::string_view SHARED_STATE_LUA_CMD =
			"shared-state";

	static constexpr std::string_view SHARED_STATE_GET_CANDIDATES_CMD =
			"shared-state-get_candidates_neigh";

	/** The message format on the wire is:
	* |     1 byte       |           |   4 bytes   |      |
	* | type name lenght | type name | data lenght | data |
	*/
	struct NetworkMessage
	{
		std::string mTypeName;
		std::vector<uint8_t> mData;
	};

	static std::task<ssize_t> receiveNetworkMessage(
	        Socket& socket, NetworkMessage& netMsg,
	        std::error_condition* errbub = nullptr );

	static std::task<ssize_t> sendNetworkMessage(
	        Socket& socket, const NetworkMessage& netMsg,
	        std::error_condition* errbub = nullptr );
};
