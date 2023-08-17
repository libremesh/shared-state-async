/*
 * Shared State
 *
 * Copyright (c) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
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

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include "io_context.hh"

#include <util/rsdebuglevel2.h>

class AsyncFileDescriptor
{
public:
	AsyncFileDescriptor(int fd, IOContext &io_context):
	    io_context_{io_context}, mFD{fd}
	{
		RS_DBG0("fd: ", fd);

		// TODO: This can fail do not do it in the costructor!!
		fcntl(mFD, F_SETFL, O_NONBLOCK);

		/* Why not attaching here?
		 * Because child classes may have speciall attacching needs,
		 * see ConnectingSocket which need attaching read only as an example */
	}

	AsyncFileDescriptor(const AsyncFileDescriptor &) = delete;
	explicit AsyncFileDescriptor(IOContext &io_context):
	    io_context_{io_context} {}

    ~AsyncFileDescriptor()
    {
		RS_DBG0("fd: ", mFD);
		if (mFD == -1)
        {
            return;
        }
        io_context_.detach(this);
		close(mFD);
    }

    bool resumeRecv()
    {
		RS_DBG0("fd: ", mFD);

        //this guard is necesary because attach method subscribes the fd to 
        //epoll but it still doses'n have a suspending coroutine waiting for the event. 
        if (!coroRecv_)
		{
			RS_DBG0("fd: ", mFD, "missing coroutine");
			return false;
		}
        coroRecv_.resume();
        return true;
    }

    bool resumeSend()
    {
		RS_DBG0("fd: ", mFD);
        //this guard is necesary because attach method subscribes the fd to 
        //epoll but it still doses'n have a suspending coroutine waiting for the event. 
        if (!coroSend_)
		{
			RS_DBG0("fd: ", mFD, " missing coroutine");
			return false;
		}
        coroSend_.resume();
        return true;
    }

	inline uint32_t getIoState() { return io_state_; }
	inline uint32_t setIoState(uint32_t state)
	{
		RS_DBG2(state);
		io_state_ = state;
		return io_state_;
	}

	inline uint32_t getNewIoState() { return io_new_state_; }
	inline uint32_t setNewIoState(uint32_t state)
	{
		RS_DBG2(state);
		io_new_state_ = state;
		return io_new_state_;
	}

	inline int getFD() { return mFD; }

#if 0
protected:
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
	friend ReadOp;
    friend IOContext;
	friend ConnectOperation;
#endif

	IOContext& io_context_;
	int mFD = -1;

    std::coroutine_handle<> coroRecv_;
    bool doneRecv_ = false;
    std::coroutine_handle<> coroSend_;

private:
	uint32_t io_state_ = 0;
	uint32_t io_new_state_ = 0;
};
