// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MEMORY_GC_H_
#define ARGON_MEMORY_GC_H_

#include <vm/datatype/arobject.h>

#include "memory.h"

namespace argon::vm::memory {
    constexpr const unsigned short kGCGenerations = 3;

    class alignas(ARGON_VM_MEMORY_QUANTUM) GCHead {
    public:
        GCHead *next;
        GCHead **prev;
        uintptr_t ref;

        datatype::ArObject *GetObject() {
            return (datatype::ArObject *) (((unsigned char *) this) + sizeof(GCHead));
        }

        [[nodiscard]] bool IsTracked() const {
            return this->prev != nullptr;
        }

        [[nodiscard]] bool IsFinalized() const {
            return ((((uintptr_t) this->next) & GCBitOffsets::FinalizedMask) >> GCBitOffsets::FinalizedShift);
        }

        [[nodiscard]] bool IsVisited() const {
            return ((((uintptr_t) this->next) & GCBitOffsets::VisitedMask) >> GCBitOffsets::VisitedShift);
        }

        [[nodiscard]] GCHead *Next() const {
            return (GCHead *) (((uintptr_t) this->next) & GCBitOffsets::AddressMask);
        }

        void SetNext(GCHead *head) {
            this->next = (GCHead *) (((uintptr_t) head) | ((uintptr_t) this->next & ~GCBitOffsets::AddressMask));
        }

        void SetFinalize(bool visited) {
            if (visited)
                this->next = (GCHead *) (((uintptr_t) this->next) | GCBitOffsets::FinalizedMask);
            else
                this->next = (GCHead *) (((uintptr_t) this->next) & ~GCBitOffsets::FinalizedMask);
        }

        void SetVisited(bool visited) {
            if (visited)
                this->next = (GCHead *) (((uintptr_t) this->next) | GCBitOffsets::VisitedMask);
            else
                this->next = (GCHead *) (((uintptr_t) this->next) & ~GCBitOffsets::VisitedMask);
        }
    };

    struct GCGeneration {
        GCHead *list;

        datatype::ArSize count;
        datatype::ArSize collected;
        datatype::ArSize uncollected;

        int threshold;
        int times;
    };

    datatype::ArObject *GCNew(datatype::ArSize length);

    datatype::ArSize Collect(unsigned short generation);

    datatype::ArSize Collect();

    GCHead *GCGetHead(datatype::ArObject *object);

    void Sweep();
}

#endif // !ARGON_MEMORY_GC_H_
