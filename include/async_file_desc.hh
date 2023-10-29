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

#include <memory>
#include <deque>
#include <fcntl.h>

#include "io_context.hh"
#include "close_operation.hh"
#include "task.hh"

#include <util/rsdebuglevel2.h>

class AsyncFileDescriptor
{
public:
	AsyncFileDescriptor(int fd, IOContext &io_context):
	    io_context_{io_context}, mFD{fd}
	{
		RS_DBG4("mFD: ", mFD);

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
		if(mFD != -1)
		{
			RS_ERR( "FD: ", mFD,
			        " Destructor called before close() report to developers!" );
			print_stacktrace();
		}
	}

	std::task<bool> close(std::error_condition* errbub = nullptr)
	{
		auto sysCloseErr = co_await CloseOperation(*this, errbub);

		if(sysCloseErr)
		{
			rs_error_bubble_or_exit(
			            rs_errno_to_condition(errno), errbub,
			            "failure closing FD: ", mFD);
			co_return false;
		}

		io_context_.discard(*this);
		mFD = -1;
		co_return true;
	}

	bool resumePendingOps()
	{
		auto numPending = mPendigOps.size();

		if(!numPending)
		{
			RS_ERR( "FD: ", mFD,
			        " attempt to resume pending operations on descriptor which"
			        " have none" );
			return false;
		}

		/* Iterate at most numPending times to avoid re-looping on coroutines
		 * that needs to wait again and are appended again on the pending queue
		 */
		for(; numPending > 0; --numPending, mPendigOps.pop_front())
			mPendigOps.front().resume();

		return true;
	}

	void addPendingOp(std::coroutine_handle<> op)
	{
		mPendigOps.push_back(op);
	}

	inline uint32_t getIoState() { return io_state_; }
	inline uint32_t setIoState(uint32_t state)
	{
		RS_DBG4(state);
		io_state_ = state;
		return io_state_;
	}

	inline uint32_t getNewIoState() { return io_new_state_; }
	inline uint32_t setNewIoState(uint32_t state)
	{
		RS_DBG4(state);
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

	/// TODO: Double check if really need this or we can implement a more
	/// elegant solution
	bool doneRecv_ = false;

private:
	uint32_t io_state_ = 0;
	uint32_t io_new_state_ = 0;

	/**
	 * @brief Keep pending operation in a queue.
	 * The new operation queuing works very well in our case, but I
	 * haven't reasoned enough if it would work also in other protocols
	 * or with multiple thread. In particular shared-state protocol
	 * is question-answere so on the same socket/file read and write
	 * never happen at same time, and are always in order one after
	 * another, this is not guaranted for every protocol but for now I
	 * got no time to think more on this */
	std::deque<std::coroutine_handle<>> mPendigOps;
};
