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
#include <system_error>
#include <ostream>

#include "task.hh"
#include "epoll_events_to_string.hh"

#include <util/rsdebug.h>
#include <util/stacktrace.h>
#include <util/rsdebuglevel2.h>

class IOContext;

class AsyncFileDescriptor
{
public:
	AsyncFileDescriptor(const AsyncFileDescriptor &) = delete;

	~AsyncFileDescriptor()
	{
		RS_DBG2(*this);
		if(mFD != -1)
		{
			RS_FATAL( *this,
			          " Destructor called before IOContext::closeAFD "
			          "report to developers!" );
			print_stacktrace();
			exit(static_cast<int>(std::errc::state_not_recoverable));
		}
	}

	/**
	 * @param events optional epoll events flags, just used for debugging
	 */
	bool resumePendingOps(uint32_t events)
	{
		auto numPending = mPendigOps.size();
		RS_DBG2(*this, " numPending: ", numPending);

		if(!numPending)
		{
			RS_DBG2( *this,
			         " attempt to resume pending operations on descriptor which"
					 " have none due: ", epoll_events_to_string(events) );
			return false;
		}

		/* Iterate at most numPending times to avoid re-looping on coroutines
		 * that needs to wait again and are re-appended on the pending queue
		 */
		for(; numPending > 0; --numPending, mPendigOps.pop_front())
			mPendigOps.front().resume();

		return true;
	}

	void addPendingOp(std::coroutine_handle<> op)
	{
		mPendigOps.push_back(op);
		RS_DBG2(*this, " numPending: ", mPendigOps.size());
	}

	inline uint32_t getIoState() const { return mIoState; }
	inline uint32_t setIoState(uint32_t state)
	{
		RS_DBG4(state);
		mIoState = state;
		return mIoState;
	}

	inline uint32_t getNextIoState() const { return mNextIOState; }
	inline uint32_t setNextIoState(uint32_t state)
	{
		RS_DBG4(state);
		mNextIOState = state;
		return mNextIOState;
	}

	inline int getFD() const { return mFD; }
	inline IOContext& getIOContext() const { return mIOContext; }

	/// TODO: Double check if really need this or we can implement a more
	/// elegant solution
	bool doneRecv_ = false;

	friend std::ostream &operator<<(
		std::ostream& out, const AsyncFileDescriptor& aFD );

protected:
	friend IOContext;
	AsyncFileDescriptor(int fd, IOContext &io_context):
	    mIOContext{io_context}, mFD{fd}
	{
		RS_DBG2(*this);

		/* Why not attaching here?
		 * Because child classes may have special attacching needs,
		 * see ConnectingSocket which need attaching read only as an example */
	}

	int mFD = -1;
	IOContext& mIOContext;

private:
	uint32_t mIoState = 0;
	uint32_t mNextIOState = 0;

	/**
	 * @brief Keep pending operation in a queue.
	 * The new operation queuing works very well in our case, but I
	 * haven't reasoned enough if it would work also in other protocols
	 * or with multiple threads. In particular shared-state protocol
	 * is question-answer so on the same socket/file read and write
	 * never happen at same time, and are always in order one after
	 * another, this is not guaranted for every protocol but for now I
	 * got no time to think more on this */
	std::deque<std::coroutine_handle<>> mPendigOps;
};
