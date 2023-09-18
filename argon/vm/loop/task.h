// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP_TASK_H_
#define ARGON_VM_LOOP_TASK_H_

#include <argon/vm/fiber.h>

#include <argon/vm/datatype/objectdef.h>

namespace argon::vm::loop {

    using TaskCB = void (*)(struct Task *);

    struct Task {
        Task *next;

        Fiber *fiber;

        TaskCB callback;
    };

    struct TimerTask : Task {
        struct {
            TimerTask *parent;
            TimerTask *left;
            TimerTask *right;
        } heap;

        datatype::ArSize id;

        datatype::ArSize timeout;

        datatype::ArSize repeat;
    };

    inline bool TimerTaskLess(const TimerTask *t1, const TimerTask *t2) {
        if (t1->timeout < t2->timeout)
            return true;

        if (t1->timeout == t2->timeout)
            return t1->id < t2->id;

        return false;
    }
} // namespace argon::vm::loop

#endif // !ARGON_VM_LOOP_TASK_H_
