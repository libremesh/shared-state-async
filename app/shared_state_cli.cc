/*
 * Shared State
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

#include "shared_state_cli.hh"

#include <iostream>
#include <iterator>
#include <string>
#include <algorithm>

#include <serialiser/rsserializable.h>
#include <serialiser/rstypeserializer.h>
#include <util/rsnet.h>

#include "shared_state_errors.hh"
#include "async_timer.hh"

using NoReturn = SharedStateCli::NoReturn;

std::task<NoReturn> SharedStateCli::insert(const std::string& typeName)
{
	const auto typesIt = mStates.find(typeName);
	if(typesIt == mStates.end())
		rs_error_bubble_or_exit(
		            SharedStateErrors::UNKOWN_DATA_TYPE, nullptr,
		            typeName );
	auto& tState = typesIt->second;

	RsJson jsonInput;

	{
		std::istreambuf_iterator<char> cinBegin(std::cin), cinEnd;
		std::string inpputStr(cinBegin, cinEnd);
		jsonInput.Parse(inpputStr.data(), inpputStr.size());
	}

	if(jsonInput.HasParseError())
	{
		RsFatal("Invalid JSON input");
		exit(EINVAL);
	}

	for(auto& member : jsonInput.GetObject())
	{
		auto& entry = tState[member.name.GetString()];
		entry.mAuthor = authorPlaceOlder();
		// Take in account merge being conservative
		entry.mTtl = mTypeConf[typeName].mBleachTTL +
		        mTypeConf[typeName].mUpdateInterval + std::chrono::seconds(1);
		entry.mData.CopyFrom(member.value, entry.mData.GetAllocator());
	}

	co_await SharedState::syncWithPeer(typeName, localInstanceAddr());

	exit(0);
}

std::task<NoReturn> SharedStateCli::dump(const std::string& typeName)
{
	auto statesIt = mStates.find(typeName);
	if(statesIt == mStates.end())
		rs_error_bubble_or_exit( SharedStateErrors::UNKOWN_DATA_TYPE, nullptr,
		                         typeName );

	auto& tState = statesIt->second;

	co_await syncWithPeer(typeName, localInstanceAddr());

	// State is empty nothing to dump
	if(tState.empty())
	{
		std::cout << "{}" << std::endl;
		exit(0);
	}

	RsGenericSerializer::SerializeJob j(RsGenericSerializer::TO_JSON);
	RsGenericSerializer::SerializeContext ctx;
	RS_SERIAL_PROCESS(tState);

	std::cout << ctx.mJson["tState"] << std::endl;

	exit(0);
}

std::task<NoReturn> SharedStateCli::get(const std::string& typeName)
{
	auto statesIt = mStates.find(typeName);
	if(statesIt == mStates.end())
		rs_error_bubble_or_exit( SharedStateErrors::UNKOWN_DATA_TYPE, nullptr,
		                         typeName );

	auto& tState = statesIt->second;

	co_await syncWithPeer(typeName, localInstanceAddr());

	// State is empty nothing to dump
	if(tState.empty())
	{
		std::cout << "{}" << std::endl;
		exit(0);
	}

	RsJson cleanJsonData(rapidjson::kObjectType);
	for(auto& [key, stateEntry]: tState)
	{
		rapidjson::Value jKey;
		jKey.SetString( key.c_str(),
		                static_cast<rapidjson::SizeType>(key.length()),
		                cleanJsonData.GetAllocator() );

		cleanJsonData.AddMember(
		            jKey, stateEntry.mData, cleanJsonData.GetAllocator() );
	}

	std::cout << prettyJSON << cleanJsonData << std::endl;

	exit(0);
}

std::task<NoReturn> SharedStateCli::acceptReqSyncConnectionsLoop(
        ListeningSocket& listener )
{
	while(true)
	{
		auto socket = co_await listener.accept();

		/* Going out of scope the returned task is destroyed, we need to
		 * detach the coroutine otherwise it will be abruptly stopped too before
		 * finishing the job */
		std::error_condition reqSyncErr;
		bool tSuccess = co_await
		        SharedState::handleReqSyncConnection(socket, &reqSyncErr);
	}
}

std::task<NoReturn> SharedStateCli::bleachDataLoop()
{
	/* Do bleach in it's own loop so bleaching is done regularly even if other
	 * operations slow down or hang up temporarly.
	 * That bleaching happens regurarly is fundamental for the network
	 * functioning. */
	std::chrono::steady_clock::time_point lastBleachCompletedTS =
	        std::chrono::steady_clock::now();
	std::error_condition tErr;
	auto asyncTimer = AsyncTimer::create(mIoContext, &tErr);

	while( asyncTimer && !tErr &&
	       co_await asyncTimer->wait(
	           std::chrono::seconds(0), std::chrono::milliseconds(999), &tErr) )
	{
		loadRegisteredTypes();

		/* If the process has been very busy we might end up being called
		 * less then once per second, if that dealy become noticeable it can be
		 * problematic for the whole network so take in account how much time
		 * has passed since last complete bleach */
		const auto elapsedTimeSinceLastBleach =
		        std::chrono::steady_clock::now() - lastBleachCompletedTS;
		const auto bleachTimes =
		        elapsedTimeSinceLastBleach > std::chrono::seconds(1) ?
		        elapsedTimeSinceLastBleach : std::chrono::seconds(1);

		for(auto&& [typeName, typeConf]: std::as_const(mTypeConf))
			bleach( typeName,
			        std::chrono::duration_cast<std::chrono::seconds>(bleachTimes) );
		lastBleachCompletedTS = std::chrono::steady_clock::now();
	}

	rs_error_bubble_or_exit(tErr, nullptr, "Bleach timer wait failed");
}


std::task<NoReturn> SharedStateCli::peer()
{
	isPeer = true;

	loadRegisteredTypes();

	auto listener = ListeningSocket::setupListener(
	            SharedState::TCP_PORT, mIoContext );

	RS_INFO("Listening on TCP port: ", SharedState::TCP_PORT, " ", *listener);

	auto acceptConnectionsTask = acceptReqSyncConnectionsLoop(*listener);
	acceptConnectionsTask.resume();

	auto bleachDataTask = SharedStateCli::bleachDataLoop();
	bleachDataTask.resume();

	std::error_condition tErr;
	auto asyncTimer = AsyncTimer::create(mIoContext, &tErr);

	while( asyncTimer && !tErr &&
	      co_await asyncTimer->wait(
	          std::chrono::seconds(0),
	          std::chrono::milliseconds(999), &tErr) )
	{
		/* TODO: Evaluate how much time skew, due to not accouinting for
		 * time spent during syncing, might affect data propagation
		 * correctness and convergence. If it causes problems in real
		 * networks adjust timer wait at each iteration depending on how
		 * much time is elapsed during the iteration */

		loadRegisteredTypes();

		const auto tNow = std::chrono::time_point_cast<std::chrono::seconds>(
		            std::chrono::steady_clock::now() );
		std::vector<std::string> shouldSyncTypes;
		for(auto&& [typeName, typeConf]: std::as_const(mTypeConf))
		{
			if( tNow.time_since_epoch().count() %
			        typeConf.mUpdateInterval.count() ) RS_LIKELY
			    continue;

			shouldSyncTypes.push_back(typeName);
		}
		if(shouldSyncTypes.empty()) continue;

		std::vector<sockaddr_storage> peersAddresses;
		co_await getCandidatesNeighbours(peersAddresses, mIoContext);

		for(auto&& typeName: std::as_const(shouldSyncTypes))
		{
			for(auto&& peerAddress : std::as_const(peersAddresses))
			{
				std::error_condition errInfo;
				bool peerSynced = co_await
				        SharedState::syncWithPeer(
				            typeName, peerAddress, &errInfo );
				RS_DBG3( peerSynced ? "Success" : "Failure",
				         " synchronizing data type: ",  typeName,
				         " with peer: ", peerAddress, " error: ", errInfo );
			}
		}
	}

	exit(tErr.value());
}


std::task<NoReturn> SharedStateCli::discover()
{
	std::vector<sockaddr_storage> discoveredPeersAddresses;

	std::error_condition mErr;
	if(! co_await SharedState::getCandidatesNeighbours(
	        discoveredPeersAddresses, mIoContext, &mErr ))
	{
		RS_FATAL("Failure discovering peers ", mErr);
		exit(mErr.value());
	}

	for(auto&& peerAddress : discoveredPeersAddresses)
		std::cout << peerAddress << std::endl;

	exit(0);
}

std::task<NoReturn> SharedStateCli::sync(
        const std::string& dataTypeName,
        const std::vector<sockaddr_storage>& pPeerAddresses )
{
	std::vector<sockaddr_storage> peerAddresses;

	// First to get local instance state
	peerAddresses.push_back(SharedState::localInstanceAddr());

	/* Peers weren't specified let's discover potential peers */
	if(pPeerAddresses.empty())
	{
		std::vector<sockaddr_storage> discoveredPeersAddresses;
		std::error_condition mErr;
		if(! co_await SharedState::getCandidatesNeighbours(
		        discoveredPeersAddresses, mIoContext, &mErr ))
		{
			RS_FATAL("Failure discovering peers ", mErr);
			exit(mErr.value());
		}

		peerAddresses.insert(
		            peerAddresses.end(),
		            discoveredPeersAddresses.begin(),
		            discoveredPeersAddresses.end() );
	}
	else
		peerAddresses.insert(
		            peerAddresses.end(),
		            pPeerAddresses.begin(),
		            pPeerAddresses.end() );

	// Last to sync collected changes back to local instance
	peerAddresses.push_back(SharedState::localInstanceAddr());

	int retval = 0;
	for(auto&& peerAddress: std::as_const(peerAddresses))
	{
		std::error_condition errInfo;
		bool peerSynced = co_await
		        syncWithPeer(dataTypeName, peerAddress, &errInfo);
		if(!peerSynced)
		{
			RS_INFO( "Failure syncronizing with peer: ", peerAddress,
			         " error: ", errInfo );
			retval = errInfo.value();
		}
	}

	if(retval)
		RS_ERR("Some errors occurred, see previous messages for details");
	exit(retval);
}

std::task<NoReturn> SharedStateCli::registerDataType(
        const std::string& typeName, const std::string& typeSope,
        std::chrono::seconds updateInterval, std::chrono::seconds TTL )
{
	SharedState::registerDataType(typeName, typeSope, updateInterval, TTL);
	exit(0);
}
