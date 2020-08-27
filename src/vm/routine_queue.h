// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_ROUTINE_QUEUE_H_
#define ARGON_VM_ROUTINE_QUEUE_H_

#include "ar_routine.h"

namespace argon::vm {

    class ArRoutineQueue {
        ArRoutine *head_ = nullptr;
        ArRoutine *tail_ = nullptr;
        unsigned int len_ = 0;
        unsigned int limit_ = 0; // unlimited

    public:
        ArRoutineQueue() = default;

        explicit ArRoutineQueue(unsigned int max_len) : limit_(max_len) {};

        ArRoutine *Dequeue();

        ArRoutine *StealQueue(unsigned int min_len, ArRoutineQueue &queue);

        bool Enqueue(ArRoutine *routine);

        unsigned int GrabHalfQueue(unsigned int min_len, ArRoutineQueue &queue);
    };

} // namespace argon::vm

#endif // !ARGON_VM_ROUTINE_QUEUE_H_
