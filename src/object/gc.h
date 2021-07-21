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

        [[nodiscard]] ArObject *GetObject() {
            return (ArObject *) (((unsigned char *) this) + sizeof(GCHead));
        }

        [[nodiscard]] GCHead *Next() const {
            return (GCHead *) (((uintptr_t) this->next) & GCBitOffsets::AddressMask);
        }

        void SetNext(GCHead *head) {
            this->next = (GCHead *) (((uintptr_t) head) | ((uintptr_t) this->next & ~GCBitOffsets::AddressMask));
        }

        [[nodiscard]] bool IsTracked() const {
            return this->prev != nullptr;
        }

        [[nodiscard]] bool IsVisited() const {
            return ((((uintptr_t) this->next) & GCBitOffsets::VisitedMask) >> GCBitOffsets::VisitedShift);
        }

        [[nodiscard]] bool IsFinalized() const {
            return ((((uintptr_t) this->next) & GCBitOffsets::FinalizedMask) >> GCBitOffsets::FinalizedShift);
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

        ArSize count;
        ArSize collected;
        ArSize uncollected;

        int threshold;
        int times;
    };

    ArObject *GCNew(ArSize len);

    ArSize Collect(unsigned short generation);

    ArSize Collect();

    ArSize STWCollect(unsigned short generation);

    ArSize STWCollect();

    bool GCIsEnabled();

    bool GCEnabled(bool enable);

    void GCFree(ArObject *obj);

    void Sweep();

    void Track(ArObject *obj);

    void ThresholdCollect();

    void UnTrack(ArObject *obj);

    inline GCHead *GCGetHead(ArObject *obj) {
        return (GCHead *) (((unsigned char *) obj) - sizeof(GCHead));
    }

    inline bool GCIsTracking(ArObject *obj) {
        if (obj->ref_count.IsGcObject())
            return GCGetHead(obj)->IsTracked();
        return false;
    }

    inline void TrackIf(ArObject *container, ArObject *item) {
        if (item->ref_count.IsGcObject() && !GCIsTracking(container))
            Track(container);
    }

    template<typename ...Items>
    inline void TrackIf(ArObject *container, ArObject *item, Items... items) {
        TrackIf(container, item);
        TrackIf(container, items...);
    }

} // namespace argon::object


#endif // !ARGON_OBJECT_GC_H_
