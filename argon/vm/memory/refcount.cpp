// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <argon/vm/datatype/arobject.h>

#include <argon/vm/memory/memory.h>
#include <argon/vm/memory/refcount.h>

using namespace argon::vm::memory;

RCObject RefCount::GetObjectBase() {
    auto obj = (argon::vm::datatype::ArObject *) this;
    assert(((void *) obj == &(obj->head_.ref_count_)) && "RefCount must be FIRST field in ArObject structure!");
    return (RCObject) obj;
}

SideTable *RefCount::AllocOrGetSideTable() {
    auto current = this->bits_.load(std::memory_order_seq_cst);
    SideTable *side;

    assert(!RC_CHECK_IS_STATIC(current));

    if (!RC_HAVE_INLINE_COUNTER(current))
        return RC_GET_SIDETABLE(current);

    if ((side = (SideTable *) Alloc(sizeof(SideTable))) == nullptr)
        return nullptr;

    side->strong.store(RC_INLINE_GET_COUNT(current));
    side->weak.store(1);
    side->object = this->GetObjectBase();

    auto desired = ((uintptr_t) side);

    if (RC_CHECK_IS_GCOBJ(current))
        desired = RC_SETBIT_GC(desired);

    do {
        if (!RC_HAVE_INLINE_COUNTER(current)) {
            Free(side);
            side = RC_GET_SIDETABLE(current);
            break;
        }

        side->strong = RC_INLINE_GET_COUNT(current);
    } while (!this->bits_.compare_exchange_weak(current, desired,
                                                std::memory_order_seq_cst,
                                                std::memory_order_seq_cst));
    return side;
}

bool RefCount::DecStrong(uintptr_t *out) {
    auto current = *((uintptr_t *) &this->bits_);
    uintptr_t desired = {};
    bool release;

    if (RC_CHECK_IS_STATIC(current))
        return false;

    do {
        desired = current;

        if (!RC_HAVE_INLINE_COUNTER(desired)) {
            auto side = RC_GET_SIDETABLE(desired);

            if (out != nullptr)
                *out = desired;

            if (side->strong.fetch_sub(1) == 1) {
                // ArObject can be destroyed
                if (side->weak.fetch_sub(1) == 1) {
                    // No weak ref! SideTable can be destroyed
                    Free(side);
                }
                return true;
            }
            return false;
        }

        desired = RC_INLINE_DEC(desired);
    } while (!this->bits_.compare_exchange_weak(current, desired,
                                                std::memory_order_release,
                                                std::memory_order_acquire));

    release = RC_CHECK_INLINE_ZERO(desired);

    if (out != nullptr)
        *out = desired;

    return release;
}

bool RefCount::DecWeak() const {
    auto current = *((uintptr_t *) &this->bits_);
    assert(!RC_HAVE_INLINE_COUNTER(current));

    auto side = RC_GET_SIDETABLE(current);
    auto weak = side->weak.fetch_sub(1);

    if (weak == 1)
        Free(side);

    return weak <= 2;
}

bool RefCount::HaveSideTable() const {
    auto current = *((uintptr_t *) &this->bits_);
    return !RC_CHECK_IS_STATIC(current) && !RC_HAVE_INLINE_COUNTER(current);
}

bool RefCount::IncStrong() {
    auto current = *((uintptr_t *) &this->bits_);
    uintptr_t desired = {};

    if (RC_CHECK_IS_STATIC(current))
        return true;

    do {
        desired = current;

        if (!RC_HAVE_INLINE_COUNTER(desired)) {
            assert(RC_GET_SIDETABLE(desired)->strong.fetch_add(1) != 0);
            return true;
        }

        assert(RC_INLINE_GET_COUNT(desired) > 0);

        desired = RC_INLINE_INC(desired);

        if (RC_CHECK_INLINE_OVERFLOW(desired)) {
            // Inline counter overflow
            auto *side = this->AllocOrGetSideTable();
            if (side == nullptr)
                return false;

            side->strong++;
            return true;
        }
    } while (!this->bits_.compare_exchange_weak(current, desired,
                                                std::memory_order_release,
                                                std::memory_order_acquire));

    return true;
}

RCObject RefCount::GetObject() {
    auto current = this->bits_.load(std::memory_order_seq_cst);

    if (RC_HAVE_INLINE_COUNTER(current)) {
        this->IncStrong();
        return this->GetObjectBase();
    }

    auto side = RC_GET_SIDETABLE(current);
    auto strong = *((uintptr_t *) &side->strong);
    uintptr_t desired;

    do {
        desired = strong + 1;
        if (desired == 1)
            return nullptr;
    } while (side->strong.compare_exchange_weak(strong, desired, std::memory_order_seq_cst));

    return side->object;
}

uintptr_t RefCount::IncWeak() {
    auto side = this->AllocOrGetSideTable();
    if (side != nullptr) {
        side->weak++;
        return (uintptr_t) side;
    }

    return 0;
}

uintptr_t RefCount::GetStrongCount() const {
    auto current = this->bits_.load(std::memory_order_seq_cst);

    if (RC_HAVE_INLINE_COUNTER(current) || RC_CHECK_IS_STATIC(current))
        return RC_INLINE_GET_COUNT(current);

    return RC_GET_SIDETABLE(current)->strong;
}

uintptr_t RefCount::GetWeakCount() const {
    auto current = this->bits_.load(std::memory_order_seq_cst);

    if (!RC_CHECK_IS_STATIC(current) && !RC_HAVE_INLINE_COUNTER(current))
        return RC_GET_SIDETABLE(current)->weak;

    return 0;
}