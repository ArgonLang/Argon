// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP2_EVENT_H_
#define ARGON_VM_LOOP2_EVENT_H_

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <winsock2.h>

#undef IN
#undef OUT
#undef Yield

#endif

#include <argon/vm/runtime.h>
#include <argon/vm/datatype/arobject.h>

namespace argon::vm::loop2 {
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
        std::mutex lock;

        struct {
            Event *parent;

            Event *left;
            Event *right;
        } heap;

        EventCB callback;

        UserCB user_callback;

        Fiber *fiber;

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

        datatype::ArSize timeout;

        std::atomic_uint refc;

        unsigned int id;

        int flags;

        bool discard_on_timeout;
    };

    inline bool EventLess(const Event *e1, const Event *e2) {
        if (e1->timeout < e2->timeout)
            return true;

        if (e1->timeout == e2->timeout)
            return e1->id < e2->id;

        return false;
    }

    class EventQueue {
        Event *head_ = nullptr;
        Event *tail_ = nullptr;

        unsigned int items = 0;

    public:
        Event *Dequeue() {
            Event *t;

            if (this->head_ == nullptr)
                return nullptr;

            t = this->head_;

            if (this->head_->heap.left != nullptr) {
                this->head_->heap.left->heap.right = nullptr;
                this->head_ = this->head_->heap.left;
            } else {
                this->head_ = nullptr;
                this->tail_ = nullptr;
            }

            this->items--;

            return t;
        }

        Event *GetHead() {
            return this->head_;
        }

        [[nodiscard]] unsigned int Count() const {
            return this->items;
        }

        void Enqueue(Event *t) {
            t->heap.right = this->tail_;
            t->heap.left = nullptr;

            if (this->tail_ != nullptr)
                this->tail_->heap.left = t;

            if (this->head_ == nullptr)
                this->head_ = t;

            this->tail_ = t;

            this->items++;
        }
    };

    class EventStack {
        Event *stack = nullptr;

        unsigned int items = 0;
    public:
        Event *Pop() {
            auto *t = this->stack;
            if (t == nullptr)
                return nullptr;

            this->stack = t->heap.right;

            this->items--;

            return t;
        }

        [[nodiscard]] unsigned int Count() const {
            return this->items;
        }

        void Push(Event *t) {
            t->heap.right = this->stack;
            this->stack = t;

            this->items++;
        }
    };
} // namespace argon::vm::loop2

#endif // !ARGON_VM_LOOP2_EVENT_H_
