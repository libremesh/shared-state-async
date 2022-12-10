#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

#include "io_context.hh"
#include "block_syscall.hh"
#include "task.hh"

class AsyncFileDescriptor
{
public:
    /* Listen tcp non blocking socket */
    AsyncFileDescriptor(IOContext& io_context): io_context_{io_context} 
    {}
    AsyncFileDescriptor(const AsyncFileDescriptor&) = delete;
    AsyncFileDescriptor(AsyncFileDescriptor&& socket)
    : io_context_{socket.io_context_}
    , io_state_{socket.io_state_}
    , io_new_state_{socket.io_new_state_}
    {
    }

    bool resumeRecv()
    {
        if (!coroRecv_)
            return false;
        coroRecv_.resume();
        return true;
    }

    bool resumeSend()
    {
        if (!coroSend_)
            return false;
        coroSend_.resume();
        return true;
    }

protected:
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
    friend FileReadOperation;
    friend IOContext;
    IOContext& io_context_;
    uint32_t io_state_ = 0;
    uint32_t io_new_state_ = 0;
    std::coroutine_handle<> coroRecv_;
    std::coroutine_handle<> coroSend_;
};
