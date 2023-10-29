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
#pragma once

#include <sys/epoll.h>
#include <string>

std::string epoll_events_to_string(uint32_t events)
{
	std::string tRet;

	auto eAdd = [&](std::string evs)
	{
		if(!tRet.empty()) tRet += ", ";
		tRet += evs;
	};

	if(events & EPOLLIN) eAdd("EPOLLIN");
	if(events & EPOLLPRI) eAdd("EPOLLPRI");
	if(events & EPOLLOUT) eAdd("EPOLLOUT");
	if(events & EPOLLRDNORM) eAdd("EPOLLRDNORM");
	if(events & EPOLLRDBAND) eAdd("EPOLLRDBAND");
	if(events & EPOLLWRNORM) eAdd("EPOLLWRNORM");
	if(events & EPOLLWRBAND) eAdd("EPOLLWRBAND");
	if(events & EPOLLMSG) eAdd("EPOLLMSG");
	if(events & EPOLLERR) eAdd("EPOLLERR");
	if(events & EPOLLHUP) eAdd("EPOLLHUP");
	if(events & EPOLLRDHUP) eAdd("EPOLLRDHUP");
	if(events & EPOLLEXCLUSIVE) eAdd("EPOLLEXCLUSIVE");
	if(events & EPOLLWAKEUP) eAdd("EPOLLWAKEUP");
	if(events & EPOLLONESHOT) eAdd("EPOLLONESHOT");
	if(events & EPOLLET) eAdd("EPOLLET");

	return tRet;
}
