// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <argon/vm/runtime.h>

#include <argon/vm/memory/memory.h>

#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/nil.h>

#include <argon/vm/loop2/evloop.h>

using namespace argon::vm::loop2;
using namespace argon::vm::datatype;

bool argon::vm::loop2::AddEvent(EvLoop *loop, Event *event, unsigned int timeout) {
    argon::vm::SetFiberStatus(FiberStatus::BLOCKED);

    event->fiber = vm::GetFiber();

    if (timeout > 0)
        event->refc++;

    loop->io_count++;

    if (event->callback(event) == CallbackStatus::FAILURE) {
        loop->io_count--;

        if (timeout > 0)
            event->refc--;

        argon::vm::SetFiberStatus(FiberStatus::RUNNING);

        EventDel(event);

        return false;
    }

    if (timeout > 0) {
        loop->lock.lock();

        event->timeout = TimeNow() + timeout;
        event->id = loop->time_id++;
        event->discard_on_timeout = true;

        loop->event_heap.Insert(event);

        loop->lock.unlock();

        loop->timer_count++;
    }

    loop->cond.notify_one();

    return true;
}

bool argon::vm::loop2::AddHandle(EvLoop *loop, EvHandle handle) {
    if (CreateIoCompletionPort(handle, loop->handle, 0, 0) == nullptr) {
        ErrorFromWinErr();

        return false;
    }

    return true;
}

bool argon::vm::loop2::EvLoopInit(EvLoop *loop) {
    memory::MemoryZero(loop, sizeof(EvLoop));

    loop->handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (loop->handle == nullptr) {
        ErrorFromWinErr();

        return false;
    }

    new(&loop->lock)std::mutex();
    new(&loop->cond)std::condition_variable();

    return true;
}

bool argon::vm::loop2::IOPoll(EvLoop *loop, unsigned long timeout) {
    auto status = CallbackStatus::FAILURE;

    Event *event;
    void *key;

    DWORD bytes;

    bool ok = GetQueuedCompletionStatus(loop->handle, &bytes, (PULONG_PTR) &key, (LPOVERLAPPED *) &event, timeout);
    if (!ok) {
        if (::GetLastError() == WAIT_TIMEOUT)
            return false;

        // Silently discard
        if (event == nullptr)
            return false;
    }

    std::unique_lock elck(event->lock);

    if (!event->discard_on_timeout || event->timeout > 0) {
        evloop_cur_fiber = event->fiber;

        if (!ok) {
            ErrorFromWinErr();

            if (event->user_callback != nullptr)
                event->user_callback(event, event->aux, -1);
        } else {
            event->buffer.wsa.len = bytes;

            if (event->callback != nullptr)
                status = event->callback(event);
            else
                vm::FiberSetAsyncResult(event->fiber, (ArObject *) Nil);
        }

        if (status == CallbackStatus::FAILURE || status == CallbackStatus::SUCCESS)
            Spawn(event->fiber);

        event->timeout = 0;
    }

    elck.unlock();

    loop->io_count--;

    EventDel(event);

    return true;
}

#endif
