/*
 * Shared State
 *
 * Copyright (c) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (c) 2023  Instituto Nacional de Tecnología Industrial
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

#include <coroutine>
#include <iostream>
#include <atomic>

#include <util/rsdebuglevel1.h>

namespace std
{
    template <typename T>
    struct task;
    namespace detail
    {
        template <typename T>

        /**
         * @brief base for constructing other promise types.
         * this class contains the most important aspect that
         * enables the construction of detachable tasks
         */
        struct promise_type_base
        {
			promise_type_base() { RS_DBG4(""); }
			~promise_type_base() { RS_DBG4(""); }
            coroutine_handle<> waiter; // who waits on this coroutine
            task<T> get_return_object();
            suspend_always initial_suspend() { return {}; }
            struct final_awaiter
            {
                bool await_ready() noexcept { return false; }
                void await_resume() noexcept {}
                template <typename promise_type>
                void await_suspend(coroutine_handle<promise_type> me) noexcept
                {
                    if (me.promise().waiter)
                        me.promise().waiter.resume();
                    else
                    {
                        me.destroy();
                    }
                }
            };
            auto final_suspend() noexcept
            {
                return final_awaiter{};
            }
            void unhandled_exception() {}
        };
        /**
         * @brief This promise type is used as return type of a typed task
         *
         * @tparam T used by task to return custom type, this promise type
         * contains a copy of the return value.
         */
        template <typename T>
        struct promise_type final : promise_type_base<T>
        {
            T result;
            void return_value(T value)
            {
                result = std::move(value);
            }
            T await_resume()
            {
                return result;
            }
            task<T> get_return_object();
        };

        template <>
        struct promise_type<void> final : promise_type_base<void>
        {
            void return_void() {}
            void await_resume() {}
            task<void> get_return_object();
        };

    }

    /**
     * @brief Basic coroutine task 
     * 
     * Basic coroutine task 
     * 
     * after creation of a task
     * 
     * "std::task<bool> echo_loop(Socket &socket);"
     * 
     * it can be coawaited
     * 
     * "bool run = co_await echo_loop(*socket);"
     * 
     * or it can be detached (and resumed)
     * 
     * echo_loop(std::move(socket)).detach();
     * 
     * or just resumed
     * 
     * echo_loop(std::move(socket)).resume()
     * 
     * @note even that is marked as no discard, this task can be 
     * deleted after using the explicit method "detach()" and 
     * ensuring subscription to the io_context. 
     * @tparam T
     */
    template <typename T = void>
    struct [[nodiscard]] task
	{
		using promise_type = detail::promise_type<T>;
		task(): mCoroutineHandle(nullptr) { RS_DBG4(""); }
		task(coroutine_handle<promise_type> handle): mCoroutineHandle(handle)
		{ RS_DBG4("", mCoroutineHandle.promise().number); }
		~task()
		{
			RS_DBG4("");
            if (mCoroutineHandle)
			{
				RS_DBG4( "Task finished? ", mCoroutineHandle.done(),
				         ", task disposable? ", mDetached );
                if (mCoroutineHandle.done() && mDetached)
                {
					RS_DBG4("do noting");
                }
                else if (mCoroutineHandle.done() || !mDetached)
                {
					RS_DBG4("Destroing m_coro");
                    mCoroutineHandle.destroy();
                }
                else
                {
					RS_DBG4("do not destroy coro");
                }
            }
        }

        bool await_ready() { return false; }
        T await_resume();
        void await_suspend(coroutine_handle<> waiter)
        {
            mCoroutineHandle.promise().waiter = waiter;
            mCoroutineHandle.resume();
        }
        
        /**
         * @brief starts the execution of the task
         * 
         */
        void resume()
        {
            mCoroutineHandle.resume();
        }

        /** @brief Resume and detach the task from the underlying coroutine.
         *  After calling this method the coroutine can keep running even after
         *  the task destruction. If this method has been called the task
         *  destructor doesn't destroy the coroutine if it hasn't finished yet.
         *  @warning This need to be used with special care. Detaching the task
         *  from the coroutine means that the caller loose control over the
         *  coroutine lifetime. This can be useful for long lived coroutines who
         *  can deal with it's own lifetime such as the one which process
         *  requests from a single socket.
         *  In detached mode, the coroutine will be self destroyed after the
         *  final suspend method has been invoked.
         */
        void detach()
        {
            mDetached = true;
            resume();
        }

    private:
        /**
         * the coroutine itself
        */
        coroutine_handle<promise_type> mCoroutineHandle;

        /** Internal state representing if the coroutine is "detached" or not.
         *  TODO: Make sure it doesn't need to be std::atomic
         */
        bool mDetached = false;
    };

    template <typename T>
    T task<T>::await_resume()
    {
        return std::move(mCoroutineHandle.promise().result);
    }
    template <>
    inline void task<void>::await_resume() {}
    namespace detail
    {
        // the formal return value of the coroutine
        template <typename T>
        task<T> promise_type<T>::get_return_object()
        {
            return task<T>{coroutine_handle<promise_type<T>>::from_promise(*this)};
        }
        inline task<void> promise_type<void>::get_return_object()
        {
            return task<void>{coroutine_handle<promise_type<void>>::from_promise(*this)};
        }
    }
}
