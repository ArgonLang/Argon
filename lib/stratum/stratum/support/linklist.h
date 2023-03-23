// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#ifndef STRATUM_LINKLIST_H_
#define STRATUM_LINKLIST_H_

#include <cstddef>

namespace stratum::support {
    template<typename T>
    class LinkedList {
        T *list = nullptr;
        unsigned int count = 0;

        void After(T *prev, T *nxt) {
            nxt->prev = &prev->next;
            nxt->next = prev->next;

            prev->next = nxt;

            if (nxt->next != nullptr)
                nxt->next->prev = &nxt->next;

            this->count++;
        }

    public:
        T *FindFree() {
            T *obj = this->list;

            while (obj != nullptr && obj->free == 0)
                obj = obj->next;

            return obj;
        }

        T *Pop() {
            T *tmp = this->list;
            if (tmp == nullptr)
                return nullptr;

            this->Remove(tmp);

            return tmp;
        }

        unsigned int Count() {
            return this->count;
        }

        void Insert(T *t) {
            if (this->list != nullptr) {
                t->next = this->list;
                this->list->prev = &t->next;
            } else
                t->next = nullptr;

            t->prev = &this->list;

            this->list = t;

            this->count++;
        }

        void Remove(T *t) {
            if (t->prev != nullptr)
                *t->prev = t->next; // t->prev is a pointer to the "next" property of the previous element!

            if (t->next != nullptr)
                t->next->prev = t->prev;

            t->next = nullptr;
            t->prev = nullptr;

            this->count--;
        }

        void Sort(T *obj) {
            T *cur = this->list;
            T *cand = nullptr;

            while (cur != nullptr && obj->free >= cur->free) {
                cand = cur;
                cur = cur->next;
            }

            if (cand != obj) {
                this->Remove(obj);

                if (cand == nullptr) {
                    this->Insert(obj);
                    return;
                }

                this->After(cand, obj);
            }
        }
    };
}

#endif // !STRATUM_LINKLIST_H_
