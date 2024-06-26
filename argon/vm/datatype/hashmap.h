// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_HASHMAP_H_
#define ARGON_VM_DATATYPE_HASHMAP_H_

#include <atomic>
#include <functional>

#include <argon/vm/datatype/objectdef.h>

namespace argon::vm::datatype {
    constexpr auto kHashMapInitialSize = 24;
    constexpr auto kHashMapLoadFactor = 0.75f;
    constexpr auto kHashMapMulFactor = 2;
    constexpr auto kHashMapFreeNodeDefault = 1024;

    template<typename K, typename V>
    struct HEntry {
        std::atomic_int ref;

        HEntry *next;
        HEntry **prev;

        HEntry *iter_next;
        HEntry *iter_prev;

        K *key;
        V value;
    };

    template<typename K, typename V>
    struct HashMap {
        HEntry<K, V> **map;
        HEntry<K, V> *free_node;
        HEntry<K, V> *iter_begin;
        HEntry<K, V> *iter_end;

        ArSize capacity;
        ArSize length;

        ArSize free_count;
        ArSize free_max;

        bool Initialize(ArSize capacity_, ArSize free_nodes) {
            this->map = (HEntry<K, V> **) argon::vm::memory::Calloc(capacity_ * sizeof(void *));
            if (this->map != nullptr) {
                this->free_node = nullptr;
                this->iter_begin = nullptr;
                this->iter_end = nullptr;

                this->capacity = capacity_;
                this->length = 0;
                this->free_count = 0;
                this->free_max = free_nodes;
            }

            return this->map != nullptr;
        }

        bool Initialize(ArSize capacity_) {
            return this->Initialize(capacity_, kHashMapFreeNodeDefault);
        }

        bool Initialize() {
            return this->Initialize(kHashMapInitialSize, kHashMapFreeNodeDefault);
        }

        bool Insert(HEntry<K, V> *entry) {
            ArSize index;

            if (!this->Resize())
                return false;

            if (!Hash((ArObject *) entry->key, &index))
                return false;

            index %= this->capacity;

            auto *slot = this->map[index];

            entry->next = slot;
            entry->prev = this->map + index;

            if (slot != nullptr)
                slot->prev = &entry->next;

            this->map[index] = entry;
            this->length++;

            this->AppendIterItem(entry);

            return true;
        }

        bool Lookup(K *key, HEntry<K, V> **entry) const {
            ArSize index;

            *entry = nullptr;

            if (!Hash((ArObject *) key, &index))
                return false;

            index %= this->capacity;

            for (HEntry<K, V> *cur = this->map[index]; cur != nullptr; cur = cur->next) {
                if (EqualStrict((const ArObject *) key, (const ArObject *) cur->key)) {
                    *entry = cur;

                    break;
                }
            }

            return true;
        }

        bool Remove(K *key, HEntry<K, V> **entry) {
            ArSize index;

            *entry = nullptr;

            if (!Hash((ArObject *) key, &index))
                return false;

            index %= this->capacity;

            for (HEntry<K, V> *cur = this->map[index]; cur != nullptr; cur = cur->next) {
                if (EqualStrict((ArObject *) key, (ArObject *) cur->key)) {
                    (*cur->prev) = cur->next;

                    if (cur->next != nullptr)
                        cur->next->prev = cur->prev;

                    this->length--;

                    this->RemoveIterItem(cur);

                    *entry = cur;

                    break;
                }
            }

            return true;
        }

        bool Resize() {
            HEntry<K, V> **new_map;
            ArSize new_cap;
            ArSize hash;

            if ((((float) this->length + 1) / ((float) this->capacity)) < kHashMapLoadFactor)
                return true;

            new_cap = this->capacity + ((this->capacity / kHashMapMulFactor) + 1);

            new_map = (HEntry<K, V> **) argon::vm::memory::Realloc(this->map, new_cap * sizeof(void *));
            if (new_map == nullptr)
                return false;

            memory::MemoryZero(new_map + this->capacity, (new_cap - this->capacity) * sizeof(void *));

            for (ArSize i = 0; i < this->capacity; i++) {
                for (HEntry<K, V> *prev = nullptr, *cur = new_map[i], *next; cur != nullptr; cur = next) {
                    Hash((ArObject *) cur->key, &hash);

                    hash %= new_cap;
                    next = cur->next;

                    if (hash == i) {
                        prev = cur;
                        continue;
                    }

                    cur->next = new_map[hash];
                    new_map[hash] = cur;
                    if (prev != nullptr)
                        prev->next = next;
                    else
                        new_map[i] = next;
                }
            }

            this->map = new_map;
            this->capacity = new_cap;

            return true;
        }

        HEntry<K, V> *AllocHEntry() {
            HEntry<K, V> *ret;

            if (this->free_count > 0) {
                ret = this->free_node;
                this->free_node = ret->next;
                this->free_count--;
            } else {
                ret = (HEntry<K, V> *) memory::Calloc(sizeof(HEntry<K, V>));
                if (ret == nullptr)
                    return nullptr;
            }

            ret->ref = 1;

            return ret;
        }

        void AppendIterItem(HEntry<K, V> *entry) {
            if (this->iter_begin == nullptr) {
                this->iter_begin = entry;
                this->iter_end = entry;
                return;
            }

            entry->iter_next = nullptr;
            entry->iter_prev = this->iter_end;
            this->iter_end->iter_next = entry;
            this->iter_end = entry;
        }

        void RemoveIterItem(HEntry<K, V> *entry) {
            if (entry->iter_prev != nullptr)
                entry->iter_prev->iter_next = entry->iter_next;
            else
                this->iter_begin = entry->iter_next;

            if (entry->iter_next != nullptr)
                entry->iter_next->iter_prev = entry->iter_prev;
            else
                this->iter_end = entry->iter_prev;

            entry->iter_next = nullptr;
            entry->iter_prev = nullptr;
        }

        void Clear(std::function<void(HEntry<K, V> *)> clear_fn) {
            HEntry<K, V> *tmp;

            for (HEntry<K, V> *cur = this->iter_begin; cur != nullptr; cur = tmp) {
                tmp = cur->iter_next;

                if (clear_fn != nullptr)
                    clear_fn(cur);

                this->RemoveIterItem(cur);
                this->FreeHEntry(cur);
            }

            this->length = 0;

            for (ArSize i = 0; i < this->capacity; i++)
                this->map[i] = nullptr;
        }

        void Finalize(std::function<void(HEntry<K, V> *)> clear_fn) {
            HEntry<K, V> *tmp;

            for (HEntry<K, V> *cur = this->iter_begin; cur != nullptr; cur = tmp) {
                tmp = cur->iter_next;

                if (clear_fn != nullptr)
                    clear_fn(cur);

                argon::vm::memory::Free(cur);
            }

            for (HEntry<K, V> *cur = this->free_node; cur != nullptr; cur = tmp) {
                tmp = cur->next;
                argon::vm::memory::Free(cur);
            }

            argon::vm::memory::Free(this->map);
        }

        void FreeHEntry(HEntry<K, V> *entry) {
            if (entry->ref.fetch_sub(1) != 1)
                return;

            entry->key = nullptr;

            if (this->free_count + 1 > this->free_max) {
                argon::vm::memory::Free(entry);
                return;
            }

            entry->next = this->free_node;
            this->free_node = entry;
            this->free_count++;
        }
    };
}

#endif // !ARGON_VM_DATATYPE_HASHMAP_H_
