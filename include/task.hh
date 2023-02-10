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

#include <coroutine>
#include <iostream>
#include <atomic>

namespace std
{
    template <typename T>
    struct task;
    namespace detail
    {
        static size_t total_;            
        template <typename T>
        struct promise_type_base
        {
            size_t number;
            promise_type_base()
            {
                total_=total_+1;
                number = total_;
                std::cout << __PRETTY_FUNCTION__ << " #" << number << std::endl;

            }
            ~promise_type_base()
            {
                std::cout << number << " -- Promise: dtor\n";
                std::cout << __PRETTY_FUNCTION__ << " #" << number << std::endl;

            }
            coroutine_handle<> waiter; // who waits on this coroutine
            task<T> get_return_object();
            suspend_always initial_suspend() { return {}; }
            struct final_awaiter
            {
                bool await_ready() noexcept { return false; }
                void await_resume() noexcept {}

				/** TODO: me seems a very compressed name, what does it stands
				 *  for ? */
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

    template <typename T = void>
	struct [[nodiscard]] task
    {
        using promise_type = detail::promise_type<T>;
        task()
		    : mCoroutineHandle{nullptr}
        {
                            std::cout << __PRETTY_FUNCTION__ << " #" << std::endl;

        }
        task(coroutine_handle<promise_type> handle)
		    : mCoroutineHandle{handle}
        {
			                std::cout << __PRETTY_FUNCTION__ << " #" << mCoroutineHandle.promise().number << std::endl;

        }
		~task()
		{
			std::cout << __PRETTY_FUNCTION__ << std::endl;
			if (mCoroutineHandle)
			{
				std::cout << "have you finished ? " << mCoroutineHandle.done() << ", task disposable = " << mDetached << std::endl;
				if (mCoroutineHandle.done() || !mDetached)
				{
					mCoroutineHandle.destroy();
					std::cout << "acabo de destruir la m_coro" << std::endl;
				}
				else
				{
					std::cout << "no destruir la m_coro" << std::endl;
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
		 *
		 *  TODO: explain who will destroy the coroutine in case of detach.
		 */
		void detach()
		{
			mDetached = true;
			resume();
		}

	private:
		coroutine_handle<promise_type> mCoroutineHandle;

		/** Internal state representing if the coroutine is detached or not.
		 *  TODO: Make sure it doesn't need to be std::atomic
		 */
		bool mDetached = false;
	};

    template <typename T>
    T task<T>::await_resume()
    {
        // usar move para && y en caso de que no lo sea no hay problema pues no lo afecta
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
