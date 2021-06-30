// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_GC_H_
#define ARGON_OBJECT_GC_H_

#include <atomic>
#include <mutex>

#include "arobject.h"
#include "bitoffset.h"

#define ARGON_OBJECT_GC_GENERATIONS 3

namespace argon::object {

    class alignas(ARGON_MEMORY_QUANTUM) GCHead {
    public:
        GCHead *next;
        GCHead **prev;
        uintptr_t ref;

        GCHead() = default;

        template<typename T>
        [[nodiscard]] T *GetObject() {
            return (T *) (((unsigned char *) this) + sizeof(GCHead));
        }

        [[nodiscard]] GCHead *Next() {
            return (GCHead *) (((uintptr_t) this->next) & ~GCBitOffsets::VisitedMask);
        }

        [[nodiscard]] bool IsTracked() {
            return this->prev != nullptr;
        }

        [[nodiscard]] bool IsVisited() {
            return ((((uintptr_t) this->next) & GCBitOffsets::VisitedMask) >> GCBitOffsets::VisitedShift);
        }

        void SetVisited(bool visited) {
            if (visited) {
                this->next = (GCHead *) (((uintptr_t) this->next) | GCBitOffsets::VisitedMask);
                return;
            }
            this->next = (GCHead *) (((uintptr_t) this->next) & ~GCBitOffsets::VisitedMask);
        }
    };

    struct GCStats {
        ArSize count;
        ArSize collected;
        ArSize uncollected;
    };

    ArSize Collect();

    ArSize Collect(unsigned short generation);

    void *GCNew(ArSize len);

    void GCFree(ArObject *obj);

    GCStats GetStats(unsigned short generation);

    void Sweep();

    void Track(ArObject *obj);

    void UnTrack(ArObject *obj);

    inline GCHead *GCGetHead(ArObject *obj) {
        return (GCHead *) (((unsigned char *) obj) - sizeof(GCHead));
    }

    inline bool GCIsTracking(ArObject *obj) {
        if (obj->ref_count.IsGcObject())
            return GCGetHead(obj)->IsTracked();
        return false;
    }
} // namespace argon::object


#endif // !ARGON_OBJECT_GC_H_
