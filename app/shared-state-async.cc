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

#include <cstdio>
#include <cstdlib>
#include <arpa/inet.h>
#include <signal.h>
#include <iostream>

#include <util/rsnet.h>

#include "socket.hh"
#include "task.hh"
#include "sharedstate.hh"

#include <util/rsdebug.h>
#include <util/stacktrace.h>
#include <util/rsdebuglevel2.h>



static CrashStackTrace gCrashStackTrace;

/* Following tasks doesn't return, and communicate via exit() status, to be used
 *  directly by the main */


/*noreturn*/ std::task<> syncWithPeers(
        std::string dataTypeName,
        const std::vector<sockaddr_storage>& peerAddressesPassed,
        IOContext& ioContext )
{
	std::vector<sockaddr_storage> discoveredPeersAddresses;

	/* Peers weren't specified let's discover potential peers */
	if(peerAddressesPassed.empty())
	{
		std::error_condition mErr;
		if(! co_await SharedState::getCandidatesNeighbours(
				discoveredPeersAddresses, ioContext, &mErr ))
		{
			RS_FATAL("Failure discovering peers ", mErr);
			exit(mErr.value());
		}
	}

	RS_DBG3("IOContext state after getCandidatesNeighbours ", ioContext);

	const std::vector<sockaddr_storage>& peerAddresses =
	        peerAddressesPassed.empty() ?
	            discoveredPeersAddresses : peerAddressesPassed;

	// This is done sequentially, but could be done in parallel for each peer
	int retval = 0;
	for(auto&& peerAddress : peerAddresses)
	{
		std::error_condition errInfo;
		bool peerSynced = co_await
		        SharedState::syncWithPeer(
		            dataTypeName, peerAddress, ioContext, &errInfo );
		if(!peerSynced)
		{
			RS_INFO( "Failure syncronizing with peer: ", peerAddress,
			         " error: ", errInfo );
			retval = errInfo.value();
		}

		RS_DBG3("IOContext state after syncWithPeer ", peerAddress, " ", ioContext);
	}

	if(retval)
		RS_ERR("Some errors occurred, see previous messages for details");
	exit(retval);
}


/*noreturn*/ std::task<> acceptReqSyncConnections(ListeningSocket& listener)
{
	while(true)
	{
		auto socket = co_await listener.accept();

		/* Going out of scope the returned task is destroyed, we need to
		 * detach the coroutine otherwise it will be abruptly stopped too before
		 * finishing the job */
		std::error_condition reqSyncErr;
		bool tSuccess = co_await
		        SharedState::handleReqSyncConnection (socket, &reqSyncErr);

		// TODO: print peer address instead of socket
		RS_INFO( tSuccess ? "Success" : "Failure", " handling reqsync on ",
		         *socket, reqSyncErr );
	}
}

/*noreturn*/ std::task<> discoverPeers(IOContext& ioContext)
{
	std::vector<sockaddr_storage> discoveredPeersAddresses;

	std::error_condition mErr;
	if(! co_await SharedState::getCandidatesNeighbours(
			discoveredPeersAddresses, ioContext, &mErr ))
	{
		RS_FATAL("Failure discovering peers ", mErr);
		exit(mErr.value());
	}

	for(auto&& peerAddress : discoveredPeersAddresses)
		std::cout << peerAddress << std::endl;

	exit(0);
}

int main(int argc, char* argv[])
{
	const auto usageFun = [&]()
	{
		std::cerr << "Usage: " << argv[0] << " OPERATION [ARGUMENTS]"
		          << std::endl
		          << "Supported operations: sync, listen" << std::endl;
	};

	if(argc < 2)
	{
		usageFun();
		return -EINVAL;
	}

	std::string operationName(argv[1]);

	if(operationName == "sync")
	{
		if(argc < 3)
		{
			std::cerr << "Usage: " << argv[0] << " " << argv[1]
			          << " DATA-TYPE [PEER-ADDRESS]..."
			          << std::endl;
			return -EINVAL;
		}

		std::string dataTypeName(argv[2]);

		std::vector<sockaddr_storage> peerAddresses;
		for(short i = 3; i < argc; ++i)
		{
			std::string peerAddrStr(argv[i]);
			sockaddr_storage peerAddr;
			if(!sockaddr_storage_inet_pton(peerAddr, peerAddrStr))
			{
				RS_FATAL("Invalid peer address: ", peerAddrStr);
				return -static_cast<int>(std::errc::bad_address);
			}

			sockaddr_storage_setport(peerAddr, SharedState::TCP_PORT);
			peerAddresses.push_back(peerAddr);
		}

		/* We expect write failures, expecially on sockets, to occur but we want
		 * to handle them where the error occurs rather than in a SIGPIPE
		 * handler */
		signal(SIGPIPE, SIG_IGN);

		auto ioContext = IOContext::setup();
		auto sendTask = syncWithPeers(dataTypeName, peerAddresses, *ioContext);
		sendTask.resume();
		ioContext->run();
	}
	else if(operationName == "listen")
	{
		/* We expect write failures, expecially on sockets, to occur but we want
		 * to handle them where the error occurs rather than in a SIGPIPE
		 * handler */
		signal(SIGPIPE, SIG_IGN);

		auto ioContext = IOContext::setup();
		auto listener = ListeningSocket::setupListener(
		            SharedState::TCP_PORT, *ioContext );

		RS_INFO("Listening on TCP port: ", SharedState::TCP_PORT, " ", *listener);

		auto t = acceptReqSyncConnections(*listener);
		t.resume();
		ioContext->run();
	}
	else if(operationName == "discover")
	{
		auto ioContext = IOContext::setup();
		auto discoverTask = discoverPeers(*ioContext);
		discoverTask.resume();
		ioContext->run();
	}
	else if(operationName == "--help" || operationName == "help")
	{
		usageFun();
		return 0;
	}
	else
	{
		std::cerr << "Unsupported operation: " << operationName << std::endl;
		return -EINVAL;
	}
}
