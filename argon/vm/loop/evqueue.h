// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <mutex>

#include <argon/util/macros.h>

#include <argon/vm/loop/support/queue.h>

#ifndef ARGON_VM_LOOP_EVQUEUE_H_
#define ARGON_VM_LOOP_EVQUEUE_H_

namespace argon::vm::loop {
#ifdef _ARGON_PLATFORM_WINDOWS

    using EvHandle = void *;

#else
    constexpr const unsigned int kMaxEvents = 50;

    using EvHandle = int;

    class Event;

    enum class EventDirection {
        IN,
        OUT
    };

    struct EventQueue {
        std::mutex lock;

        EventQueue *next;

        support::Queue<Event> in_events;

        support::Queue<Event> out_events;

        EvHandle handle;
    };
#endif
} // namespace argon::vm::loop

#endif //ARGON_VM_LOOP_EVQUEUE_H_
