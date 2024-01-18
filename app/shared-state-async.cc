/*
 * Shared State
 *
 * Copyright (C) 2023-2024  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include "io_context.hh"

#include <cstdio>
#include <cstdlib>
#include <arpa/inet.h>
#include <signal.h>
#include <iostream>

#include <util/rsnet.h>

#include "task.hh"
#include "shared_state_cli.hh"

#include <util/rsdebug.h>
#include <util/stacktrace.h>
#include <util/rsdebuglevel2.h>


static CrashStackTrace gCrashStackTrace;

int main(int argc, char* argv[])
{
	const auto usageFun = [&]()
	{
		std::cerr << "Usage: " << argv[0] << " OPERATION [ARGUMENTS]"
		          << std::endl
		          << "Supported operations: "
		             "discover, dump, get, insert, peer, register, sync"
		          << std::endl;
	};

	if(argc < 2)
	{
		usageFun();
		return -EINVAL;
	}

	const std::string operationName(argv[1]);

	if(operationName == "--help" || operationName == "help")
	{
		usageFun();
		return 0;
	}

	std::task<SharedStateCli::NoReturn> pendingTask;
	auto ioContext = IOContext::setup();
	SharedStateCli sharedState(*ioContext);

	auto mainRun = [&](auto&& pTask)
	{
		pendingTask = pTask;
		pendingTask.resume();
		ioContext->run();
	};

	if(operationName == "discover")
		mainRun(pendingTask = sharedState.discover());

	if(operationName == "peer")
	{
		/* We expect write failures, expecially on sockets, to occur but we want
		 * to handle them where the error occurs rather than in a SIGPIPE
		 * handler */
		signal(SIGPIPE, SIG_IGN);
		mainRun(sharedState.peer());
	}

	if(argc < 3)
	{
		std::cerr << "Operation: " << argv[1] << " needs data-type"
		          << std::endl;
		return -EINVAL;
	}

	const std::string dataTypeName(argv[2]);

	if(operationName == "dump")
		mainRun(sharedState.dump(dataTypeName));

	if(operationName == "get")
		mainRun(sharedState.get(dataTypeName));

	if(operationName == "insert")
		mainRun(sharedState.insert(dataTypeName));

	if(operationName == "sync")
	{
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

		mainRun(sharedState.sync(dataTypeName, peerAddresses));
	}

	if(operationName == "register")
	{
		if(argc != 6)
		{
			std::cerr << "Usage: " << argv[0] << " " << argv[1]
			          << " DATA-TYPE TYPE-SCOPE UPDATE-INTERVAL BLEACH-TTL"
			          << std::endl;
			return -EINVAL;
		}

		const std::string typeSope(argv[3]);
		const std::chrono::seconds updateInterval(std::stoul(argv[4]));
		const std::chrono::seconds bleachTTL(std::stoul(argv[5]));

		mainRun(sharedState.registerDataType(
		            dataTypeName, typeSope, updateInterval, bleachTTL ));
	}

	std::cerr << "Unsupported operation: " << operationName << std::endl;
	return -EINVAL;
}
