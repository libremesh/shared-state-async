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
#include "socket.hh"
#include "file_read_operation.hh"
#include "sharedstate.hh"

#include <cstdio>
#include <cstdlib>
#include <arpa/inet.h>

#include <util/rsdebug.h>
#include <util/stacktrace.h>

static CrashStackTrace gCrashStackTrace;

/* TODO: Remove copy pasted sockaddr_* functions, instead use the directly from
 * libretroshare which is downloaded automaticalle by CMake */

void sockaddr_storage_clear(struct sockaddr_storage &addr)
{
	memset(&addr, 0, sizeof(addr));
}

bool sockaddr_storage_ipv4_to_ipv6(sockaddr_storage &addr)
{
#ifdef SS_DEBUG
	std::cerr << __PRETTY_FUNCTION__ << std::endl;
#endif

	if ( addr.ss_family == AF_INET6 ) return true;

	if ( addr.ss_family == AF_INET )
	{
		sockaddr_in & addr_ipv4 = (sockaddr_in &) addr;
		sockaddr_in6 & addr_ipv6 = (sockaddr_in6 &) addr;

		uint32_t ip = addr_ipv4.sin_addr.s_addr;
		uint16_t port = addr_ipv4.sin_port;

		sockaddr_storage_clear(addr);
		addr_ipv6.sin6_family = AF_INET6;
		addr_ipv6.sin6_port = port;
		addr_ipv6.sin6_addr.s6_addr16[5] = htons(0xffff);
		memmove( reinterpret_cast<void*>(&(addr_ipv6.sin6_addr.s6_addr16[6])),
		         reinterpret_cast<void*>(&ip), 4 );
		return true;
	}

	return false;
}

bool sockaddr_storage_inet_pton( sockaddr_storage &addr,
                                 const std::string& ipStr )
{
#ifdef SS_DEBUG
	std::cerr << __PRETTY_FUNCTION__ << std::endl;
#endif

	struct sockaddr_in6 * addrv6p = (struct sockaddr_in6 *) &addr;
	struct sockaddr_in * addrv4p = (struct sockaddr_in *) &addr;

	if ( 1 == inet_pton(AF_INET6, ipStr.c_str(), &(addrv6p->sin6_addr)) )
	{
		addr.ss_family = AF_INET6;
		return true;
	}
	else if ( 1 == inet_pton(AF_INET, ipStr.c_str(), &(addrv4p->sin_addr)) )
	{
		addr.ss_family = AF_INET;
		return sockaddr_storage_ipv4_to_ipv6(addr);
	}

	return false;
}

struct sockaddr_in *to_ipv4_ptr(struct sockaddr_storage &addr)
{
	struct sockaddr_in *ipv4_ptr = (struct sockaddr_in *) &addr;
	return ipv4_ptr;
}

struct sockaddr_in6 *to_ipv6_ptr(struct sockaddr_storage &addr)
{
	struct sockaddr_in6 *ipv6_ptr = (struct sockaddr_in6 *) &addr;
	return ipv6_ptr;
}

bool sockaddr_storage_ipv4_setport(struct sockaddr_storage &addr, uint16_t port)
{
	struct sockaddr_in *ipv4_ptr = to_ipv4_ptr(addr);
	ipv4_ptr->sin_port = htons(port);
	return true;
}

bool sockaddr_storage_ipv6_setport(struct sockaddr_storage &addr, uint16_t port)
{
	struct sockaddr_in6 *ipv6_ptr = to_ipv6_ptr(addr);
	ipv6_ptr->sin6_port = htons(port);
	return true;
}

bool sockaddr_storage_setport(struct sockaddr_storage &addr, uint16_t port)
{
	switch(addr.ss_family)
	{
	    case AF_INET:
		    return sockaddr_storage_ipv4_setport(addr, port);
		    break;
	    case AF_INET6:
		    return sockaddr_storage_ipv6_setport(addr, port);
		    break;
	    default:
		    std::cerr << "sockaddr_storage_setport() invalid addr.ss_family" << std::endl;
		    break;
	}
	return false;
}

std::task<> sendStdInput(
        std::string dataTypeName, std::string peerAddrStr,
        IOContext& ioContext )
{
	SharedState::NetworkMessage netMessage;

#ifdef GIO_DUMMY_TEST
	netMessage.mTypeName = dataTypeName;
	netMessage.mData = "cacapisciapuzza";
#else
	auto flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	auto aStdIn = std::make_shared<AsyncFileDescriptor>(
	            STDIN_FILENO, ioContext );

	netMessage.mData.clear();
	netMessage.mData.resize(SharedState::DATA_MAX_LENGHT);


	bool finish = false;
	std::size_t totalRead = 0;
	while(!finish)
	{
		totalRead += co_await ReadOp(
		            aStdIn,
		            reinterpret_cast<uint8_t*>(netMessage.mData.data()),
		            netMessage.mData.length() - totalRead );
		finish = true;
	}
	fcntl(STDIN_FILENO, F_SETFL, flags);
	netMessage.mData.resize(totalRead);

	RS_DBG2( "netMessage.mTypeName: ", netMessage.mTypeName,
	         " netMessage.mData:\n", netMessage.mData );
#endif

	sockaddr_storage peerAddr;
	sockaddr_storage_inet_pton(peerAddr, peerAddrStr);
	sockaddr_storage_setport(peerAddr, 3490);

	auto socket = co_await ConnectingSocket::connect(peerAddr, ioContext);

	auto totalSent = co_await
	        SharedState::sendNetworkMessage(*socket.get(), netMessage);

	co_await SharedState::receiveNetworkMessage(*socket.get(), netMessage);

	std::cout << netMessage.mData << std::endl;

	exit(0);
}

int main(int argc, char* argv[])
{
	if(argc < 3)
	{
		RS_FATAL("Need type name and peer IP address");
		return -EINVAL;
	}

	auto ioContext = IOContext::setup();

	std::string dataTypeName(argv[1]);
	std::string peerAddrStr(argv[2]);

	RS_INFO("Got dataTypeName: ", dataTypeName, " peerAddrStr: ", peerAddrStr);

	auto sendTask = sendStdInput(dataTypeName, peerAddrStr, *ioContext.get());
	sendTask.resume();

	ioContext->run();
	return 0;
}
