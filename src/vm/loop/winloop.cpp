// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "util/macros.h"

#ifdef _ARGON_PLATFORM_WINDOWS

#include <windows.h>

#undef CONST
#undef FASTCALL
#undef Yield

#include <vm/runtime.h>

#include <vm/memory/memory.h>

#include <vm/datatype/error.h>

#include "event.h"
#include "evloop.h"

using namespace argon::vm::loop;
using namespace argon::vm::datatype;

EvLoop *argon::vm::loop::EventLoopNew() {
    auto *evl = (EvLoop *) argon::vm::memory::Alloc(sizeof(EvLoop));

    if (evl != nullptr) {
        evl->handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (evl->handle == nullptr) {
            ErrorFromWinErr();

            argon::vm::memory::Free(evl);

            return nullptr;
        }
    }

    return evl;
}

bool argon::vm::loop::EventLoopIOPoll(EvLoop *loop, unsigned long timeout) {
    Event *event;
    void *key;

    DWORD bytes;

    bool ok = GetQueuedCompletionStatus(loop->handle, &bytes, (PULONG_PTR) &key, (LPOVERLAPPED *) &event, timeout);

    thlocal_event = event;

    if (!ok) {
        if (::GetLastError() == WAIT_TIMEOUT)
            return false;

        ErrorFromWinErr();
    } else {
        event->buffer.wsa.len = bytes;

        if (event->callback != nullptr)
            event->callback(event);
        else
            ReplaceFrameTopValue(event->initiator); // Default: Set initiator as return value
    }

    Spawn(event->fiber);

    EventDel(event);

    return true;
}

bool argon::vm::loop::EventLoopIOAdd(EvLoop *loop, EvHandle handle) {
    if (CreateIoCompletionPort(handle, loop->handle, 0, 0) == nullptr) {
        ErrorFromWinErr();

        return false;
    }

    return true;
}

#endif