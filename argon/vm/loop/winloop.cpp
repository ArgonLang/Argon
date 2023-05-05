// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <argon/vm/runtime.h>

#include <argon/vm/memory/memory.h>

#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/nil.h>

#include <argon/vm/loop/event.h>
#include <argon/vm/loop/evloop.h>

using namespace argon::vm::loop;
using namespace argon::vm::datatype;

EvLoop *argon::vm::loop::EventLoopNew() {
    auto *evl = (EvLoop *) argon::vm::memory::Calloc(sizeof(EvLoop));

    if (evl != nullptr) {
        evl->handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (evl->handle == nullptr) {
            ErrorFromWinErr();

            argon::vm::memory::Free(evl);

            return nullptr;
        }

        new(&evl->lock)std::mutex();
        new(&evl->cond)std::condition_variable();
    }

    return evl;
}

bool argon::vm::loop::EventLoopIOPoll(EvLoop *loop, unsigned long timeout) {
    auto status = CallbackReturnStatus::SUCCESS;

    Event *event;
    void *key;

    DWORD bytes;

    bool ok = GetQueuedCompletionStatus(loop->handle, &bytes, (PULONG_PTR) &key, (LPOVERLAPPED *) &event, timeout);

    thlocal_event = event;

    if (!ok) {
        if (::GetLastError() == WAIT_TIMEOUT)
            return false;

        ErrorFromWinErr();

        if (event->user_callback != nullptr)
            event->user_callback(event, event->aux, -1);
    } else {
        event->buffer.wsa.len = bytes;

        if (event->callback != nullptr)
            status = event->callback(event);
        else
            vm::FiberSetAsyncResult(event->fiber, (ArObject *) Nil); // Default: Set initiator as return value
    }

    loop->io_count--;

    if (status != CallbackReturnStatus::SUCCESS_NO_WAKEUP)
        Spawn(event->fiber);

    EventDel(event);

    return true;
}

bool argon::vm::loop::EventLoopAddEvent(EvLoop *loop, Event *event) {
    argon::vm::SetFiberStatus(FiberStatus::BLOCKED);

    event->fiber = vm::GetFiber();

    if (event->callback(event) == CallbackReturnStatus::FAILURE) {
        argon::vm::SetFiberStatus(FiberStatus::RUNNING);

        EventDel(event);

        return false;
    }

    loop->io_count++;

    loop->cond.notify_one();

    return true;
}

bool argon::vm::loop::EventLoopAddHandle(EvLoop *loop, EvHandle handle) {
    if (CreateIoCompletionPort(handle, loop->handle, 0, 0) == nullptr) {
        ErrorFromWinErr();

        return false;
    }

    return true;
}

#endif