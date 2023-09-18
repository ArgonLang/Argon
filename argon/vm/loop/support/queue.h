// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP_SUPPORT_QUEUE_H_
#define ARGON_VM_LOOP_SUPPORT_QUEUE_H_

namespace argon::vm::loop::support {
    template<typename T>
    class Queue {
        T *head_ = nullptr;
        T *tail_ = nullptr;

        unsigned int items = 0;
    public:
        T *Dequeue() {
            T *t;

            if (this->head_ == nullptr)
                return nullptr;

            t = this->head_;

            if (this->head_->prev != nullptr) {
                this->head_->prev->next = nullptr;
                this->head_ = this->head_->prev;
            } else {
                this->head_ = nullptr;
                this->tail_ = nullptr;
            }

            this->items--;

            return t;
        }

        T *GetHead() {
            return this->head_;
        }

        unsigned int Count() {
            return this->items;
        }

        void Enqueue(T *t) {
            t->next = this->tail_;
            t->prev = nullptr;

            if (this->tail_ != nullptr)
                this->tail_->prev = t;

            if (this->head_ == nullptr)
                this->head_ = t;

            this->tail_ = t;

            this->items++;
        }
    };
} // namespace argon::vm::loop::support

#endif // !ARGON_VM_LOOP_SUPPORT_QUEUE_H_
