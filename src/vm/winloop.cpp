// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

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

using namespace argon::vm;
using namespace argon::vm::datatype;

EvLoop *argon::vm::EvLoopNew(int event_size) {
    auto *evl = (EvLoop *) memory::Alloc(sizeof(EvLoop));

    if (evl != nullptr) {
        if ((evl->handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0)) == nullptr) {
            ErrorFromWinErr();

            memory::Free(evl);

            return nullptr;
        }

        evl->event_size = event_size;
    }

    return evl;
}

bool argon::vm::EventPool(EvLoop *loop, Event **out_event, unsigned long timeout) {
    DWORD bytes;
    void *key;

    if (!GetQueuedCompletionStatus(loop->handle, &bytes, (PULONG_PTR) &key, (LPOVERLAPPED *) out_event, timeout))
        return ::GetLastError() == WAIT_TIMEOUT;

    return true;
}

bool argon::vm::EvLoopRegister(EvLoop *loop, EvHandle handle) {
    if (CreateIoCompletionPort(handle, loop->handle, 0, 0) == nullptr) {
        ErrorFromWinErr();

        return false;
    }

    return true;
}

#endif