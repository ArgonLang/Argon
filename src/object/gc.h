// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_GC_H_
#define ARGON_OBJECT_GC_H_

#include <atomic>
#include <mutex>

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

    class GC {
        GCHead generation_[ARGON_OBJECT_GC_GENERATIONS] = {};

        GCHead garbage_ = {};

        std::mutex track_lck_;

        std::mutex garbage_lck_;

        void SearchRoots(unsigned short generation);

        void TraceRoots(GCHead *unreachable, unsigned short generation);

    public:
        void Collect();

        void Collect(unsigned short generation);

        void Sweep();

        void Track(struct ArObject *obj);
    };

    inline GCHead *GCGetHead(struct ArObject *obj) {
        return (GCHead *) (((unsigned char *) obj) - sizeof(GCHead));
    }

    bool GCIsTracking(struct ArObject *obj);

    void *GCNew(size_t len);
} // namespace argon::object


#endif // !ARGON_OBJECT_GC_H_
