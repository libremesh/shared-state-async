/*
 * Shared State CLI
 *
 * Copyright (C) 2024  Gioacchino Mazzurco <gio@eigenlab.org>
 * Copyright (C) 2024  Asociación Civil Altermundi <info@altermundi.net>
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

#include "sharedstate.hh"
#include "io_context.hh"

struct SharedStateCli: SharedState
{
	explicit SharedStateCli(IOContext& ioContext) : SharedState(ioContext)
	{ loadRegisteredTypes(); }


	/** Similar meaning as [[noreturn]] but only for reader benefit, used as in
	 *  co-routines that exit the process on finish.
	 *  example: std::task<NoReturn> doSomethingThenExit(); */
	typedef void NoReturn;

	/** Discover potencial peers and print their addresses to standard output
	 *  then exit */
	std::task<NoReturn> discover();

	/** Dump type full shared-state including, TTL, authors etc.to standard
	 *  output. Useful to save
	 *  current state to a persistent storage or for ispection. */
	std::task<NoReturn> dump(const std::string& typeName);

	/** Get clean data out of current shared-state */
	std::task<NoReturn> get(const std::string& typeName);

	/** @brief Insert data entries into shared-state
	 * Removal on request is not possible, entries will fade away by bleaching
	 * depending on type TTL, still it is possible to insert a null value which
	 * can be considered as a remove equivalent for most types */
	std::task<NoReturn> insert(const std::string& typeName);

	std::task<NoReturn> peer();

	std::task<NoReturn> registerDataType(
	        const std::string& typeName, const std::string& typeSope,
	        std::chrono::seconds updateInterval, std::chrono::seconds TTL );

	/**
	 * @param peerAddresses address to sync with, if empty an attempt to
	 * discover peers is done internally
	 */
	std::task<NoReturn> sync( const std::string& typeName,
	                          const std::vector<sockaddr_storage>& peerAddresses );

protected:
	std::task<NoReturn> acceptReqSyncConnectionsLoop(ListeningSocket& listener);
};
