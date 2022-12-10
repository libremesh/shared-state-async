#pragma once

#include <cerrno>
#include <coroutine>
#include <type_traits>

#include <iostream>

template<typename SyscallOpt, typename ReturnValue>
class BlockSyscall //Awaiter
{
public:
    BlockSyscall()
        : haveSuspend_{false}
    {}

    bool await_ready() const noexcept { return false; }

    bool await_suspend(std::coroutine_handle<> awaitingCoroutine)
    {
        static_assert(std::is_base_of_v<BlockSyscall, SyscallOpt>);
        awaitingCoroutine_ = awaitingCoroutine;
        //casteo en tiempo de compilacion para no tener que resolver en runtime
        returnValue_ = static_cast<SyscallOpt*>(this)->syscall();
        haveSuspend_ =
            returnValue_ == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);
        //haveSuspend_=true;
        if (haveSuspend_)
            static_cast<SyscallOpt*>(this)->suspend();
            // the value true returns control to the caller/resumer of the current coroutine 

        return haveSuspend_;
        // the value false resumes the current coroutine. 
    }

    ReturnValue await_resume()
    {
        std::cout << "await_resume\n";
        if (haveSuspend_)
            returnValue_ = static_cast<SyscallOpt*>(this)->syscall();
        return returnValue_;
    }
    //derived clases must implement these methods
    //virtual ssize_t syscall() = 0;
    //virtual void suspend() = 0;

protected:
    bool haveSuspend_;
    std::coroutine_handle<> awaitingCoroutine_;
    ReturnValue returnValue_;
};
