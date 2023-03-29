#pragma once

#include <cerrno>
#include <coroutine>
#include <memory>
#include <type_traits>
#include <iostream>
#include "debug/rsdebuglevel2.h"
#include <errno.h>

template <typename SyscallOpt, typename ReturnValue>
class BlockSyscall // Awaiter
{
public:
    BlockSyscall(std::shared_ptr<std::error_condition> ec = nullptr)
        : haveSuspend_{false}, errorconditionstorage_{ec}

    {
        RS_DBG0("haveSuspend_");
    }

    bool await_ready() const noexcept
    {
        RS_DBG0("await_ready");
        return false;
    }

    bool await_suspend(std::coroutine_handle<> awaitingCoroutine)
    {
        RS_DBG0("await_suspend");

        static_assert(std::is_base_of_v<BlockSyscall, SyscallOpt>);
        awaitingCoroutine_ = awaitingCoroutine;
        returnValue_ = static_cast<SyscallOpt *>(this)->syscall();
        haveSuspend_ =
            returnValue_ == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);
        if (haveSuspend_)
        {
            RS_WARN("...suspendiendo ... por un -1");
            static_cast<SyscallOpt *>(this)->suspend();
            // the haveSuspend_ true returns control to the caller/resumer of the current coroutine
        }
        else if (returnValue_ == -1)
        {
            /// the haveSuspend_ false resumes the current coroutine. but the system call has failed..
            /// the caller has to be notified
            rs_error_bubble_or_exit(
                rs_errno_to_condition(errno), errorconditionstorage_,
                "A syscall has failed");
        }
        return haveSuspend_;
        // the haveSuspend_ false resumes the current coroutine.
    }

    ReturnValue await_resume()
    {
        RS_DBG0("await_resume");
        if (haveSuspend_)
            returnValue_ = static_cast<SyscallOpt *>(this)->syscall();
        return returnValue_;
    }
    // derived clases must implement these methods
    // virtual ssize_t syscall() = 0;
    // virtual void suspend() = 0;

protected:
    bool haveSuspend_;
    std::shared_ptr<std::error_condition> errorconditionstorage_;
    std::coroutine_handle<> awaitingCoroutine_;
    ReturnValue returnValue_;
};
