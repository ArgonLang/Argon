// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MEMORY_GC_H_
#define ARGON_MEMORY_GC_H_

#include <argon/vm/datatype/objectdef.h>

#include <argon/vm/memory/memory.h>

#define GC_GET_HEAD(ptr) ((argon::vm::memory::GCHead *) (((unsigned char *) ptr) - sizeof(argon::vm::memory::GCHead)))

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

    datatype::ArObject *GCNew(datatype::ArSize length, bool track);

    datatype::ArSize Collect(unsigned short generation);

    datatype::ArSize Collect();

    bool GCEnable(bool enable);

    bool GCIsEnabled();

    GCHead *GCGetHead(datatype::ArObject *object);

    void GCFree(datatype::ArObject *object);

    inline void GCFreeRaw(datatype::ArObject *object) {
        memory::Free(GCGetHead(object));
    }

    void Sweep();

    void ThresholdCollect();

    void Track(datatype::ArObject *object);

    void TrackIf(datatype::ArObject *track, datatype::ArObject *gc_object);
}


#endif // !ARGON_MEMORY_GC_H_
