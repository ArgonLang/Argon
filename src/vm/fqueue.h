// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_FQUEUE_H_
#define ARGON_VM_FQUEUE_H_

#include <thread>

#include "fiber.h"

namespace argon::vm {
    class FiberQueue {
        //                                            +----head
        //                                            v
        //           +--------+    +--------+    +--------+
        //           |        |    |        |    |        |
        // tail ---> |  obj3  +--->|  obj2  +--->|  obj1  |
        //           |        |    |        |    |        |
        //           +--------+    +--------+    +--------+

        Fiber *head_ = nullptr;
        Fiber *tail_ = nullptr;

        unsigned int items_ = 0;

        std::mutex lock_;

    public:
        /**
         * @brief Remove fiber from the queue.
         *
         * @return Fiber if present, nullptr otherwise.
         */
        Fiber *Dequeue();

        /**
         * @brief Insert fiber into the queue.
         *
         * @param fiber to insert.
         */
        void Enqueue(Fiber *fiber);

        /**
         * @brief Insert Fiber on the head of the queue.
         *
         * If the Dequeue function is called this is the first element to come out (it behaves like a stack).
         * @param fiber to insert.
         */
        void InsertHead(Fiber *fiber);

        /**
         * @brief Allows a node in any position to leave the queue.
         * @param fiber that wants to leave the queue.
         */
        void Relinquish(Fiber *fiber);
    };
} // namespace argon::vm

#endif // !ARGON_VM_FQUEUE_H_
