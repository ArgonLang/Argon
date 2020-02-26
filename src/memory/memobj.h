// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MEMORY_MEMOBJ_H_
#define ARGON_MEMORY_MEMOBJ_H_

#include <mutex>

namespace argon::memory {
    template<typename T>
    class MemoryObject {
        void Insert(T *obj, T *pos) {
            obj->prev = pos;
            obj->next = pos->next;
            pos->next = obj;
            if (obj->next != nullptr)
                obj->next->prev = obj;
            else
                tail = obj;

            this->count++;
        }

    public:
        std::mutex lock;
        T *head = nullptr;
        T *tail = nullptr;
        unsigned int count = 0;

        T *FindFree() {
            T *obj = nullptr;
            for (obj = this->head; obj != nullptr && obj->free == 0; obj = obj->next);
            return obj;
        }

        void Append(T *obj) {
            this->count++;

            if (this->head == nullptr) {
                head = obj;
                tail = obj;
                return;
            }

            obj->prev = tail;
            tail->next = obj;
            tail = obj;
        }

        void Remove(T *obj) {
            if (obj->prev != nullptr)
                obj->prev->next = obj->next;
            else
                head = obj->next;

            if (obj->next != nullptr)
                obj->next->prev = obj->prev;
            else
                tail = obj->prev;

            this->count--;
        }

        void Sort(T *obj) {
            T *cand = obj;

            for (T *cur = obj; cur != nullptr && obj->free >= cur->free; cand = cur, cur = cur->next);

            if (cand != obj) {
                Remove(obj);
                Insert(obj, cand);
            }
        }
    };
} // namespace argon::memory

#endif // !ARGON_MEMORY_MEMOBJ_H_
