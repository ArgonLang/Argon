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

        unsigned int max_ = 0;
        unsigned int items_ = 0;

        std::mutex lock_;

    public:
        FiberQueue() = default;

        explicit FiberQueue(int max_length) : max_(max_length) {}

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
         * @return Returns true if the item has been added to the queue,
         * otherwise false(the maximum number of items in the queue reached)
         */
        bool Enqueue(Fiber *fiber);

        /**
         * @brief Check if queue is empty or not.
         *
         * @return true if empty, false otherwise.
         */
        bool IsEmpty();

        /**
         * @brief Insert Fiber on the head of the queue.
         *
         * If the Dequeue function is called this is the first element to come out (it behaves like a stack).
         *
         * @param fiber to insert.
         * @return Returns true if the item has been added to the queue,
         * otherwise false(the maximum number of items in the queue reached)
         */
        bool InsertHead(Fiber *fiber);

        /**
         * @brief Allows a node in any position to leave the queue.
         *
         * @param fiber that wants to leave the queue.
         */
        void Relinquish(Fiber *fiber);

        /**
         * @brief Sets a limit of fibers that can be present in the Queue at the same time.
         * @param max_items max number of fiber (0 = unlimited).
         */
        void SetLimit(unsigned int max_items) {
            this->max_ = max_items;
        }
    };
} // namespace argon::vm

#endif // !ARGON_VM_FQUEUE_H_
