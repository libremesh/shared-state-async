#pragma once

// A simple version of cppcoro::task working with g++10

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
    struct task
    {
        using promise_type = detail::promise_type<T>;
        task()
            : handle_{nullptr}
        {
                            std::cout << __PRETTY_FUNCTION__ << " #" << std::endl;

        }
        task(coroutine_handle<promise_type> handle)
            : handle_{handle}
        {
                            std::cout << __PRETTY_FUNCTION__ << " #" << handle_.promise().number << std::endl;

        }
        ~task()
        {
            std::cout << "task destroy " << std::endl;
            std::cout << __PRETTY_FUNCTION__ <<  std::endl;

            if (handle_)
            {
                std::cout << "efective task #" << handle_.promise().number <<"  destroy, done ? " << handle_.done() << std::endl;
                //if(handle_.done())
                //{
                    handle_.destroy();
                //}
            }
            else
            {
                std::cout << "no hanle" << std::endl;
            }
        }

        bool await_ready() { return false; }
        T await_resume();
        void await_suspend(coroutine_handle<> waiter)
        {
            handle_.promise().waiter = waiter;
            handle_.resume();
        }

        void resume()
        {
            handle_.resume();
        }
        coroutine_handle<promise_type> handle_;
    };

    template <typename T>
    T task<T>::await_resume()
    {
        // usar move para && y en caso de que no lo sea no hay problema pues no lo afecta
        return std::move(handle_.promise().result);
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