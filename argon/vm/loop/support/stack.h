// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP_SUPPORT_STACK_H_
#define ARGON_VM_LOOP_SUPPORT_STACK_H_

namespace argon::vm::loop::support {
    template<typename T>
    class Stack {
        T *stack = nullptr;

        unsigned int items = 0;
    public:
        T *Pop() {
            auto *t = this->stack;
            if (t == nullptr)
                return nullptr;

            this->stack = t->next;

            this->items--;

            return t;
        }

        unsigned int Count() {
            return this->items;
        }

        void Push(T *t) {
            t->next = this->stack;
            this->stack = t;

            this->items++;
        }
    };
} // namespace argon::vm::loop::support

#endif // !ARGON_VM_LOOP_SUPPORT_STACK_H_
