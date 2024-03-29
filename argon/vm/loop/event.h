// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP_EVENT_H_
#define ARGON_VM_LOOP_EVENT_H_

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <winsock2.h>

#endif

#include <argon/vm/datatype/arobject.h>

namespace argon::vm::loop {
    enum class CallbackStatus {
        CONTINUE,
        FAILURE,
        RETRY,
        SUCCESS
    };

    using EventCB = CallbackStatus (*)(struct Event *);

    using UserCB = CallbackStatus (*)(struct Event *, datatype::ArObject *, int status);

#ifdef _ARGON_PLATFORM_WINDOWS
    struct Event : OVERLAPPED {
#else
    struct Event {
#endif
        Event *next;
        Event *prev;

        struct EvLoop *loop;

        struct Fiber *fiber;

        EventCB callback;

        UserCB user_callback;

        datatype::ArObject *aux;

        datatype::ArObject *initiator;

        struct {
            datatype::ArBuffer arbuf;

            unsigned char *data;

            datatype::ArSize length;
            datatype::ArSize allocated;

#ifdef _ARGON_PLATFORM_WINDOWS
            WSABUF wsa;
#endif
        } buffer;

        int flags;
    };

} // namespace argon::vm::loop

#endif // !ARGON_VM_LOOP_EVENT_H_
