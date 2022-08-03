// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/datatype/arobject.h>

#include "memory.h"
#include "refcount.h"

using namespace argon::vm::memory;

RCObject RefCount::GetObjectBase() {
    auto obj = (argon::vm::datatype::ArObject *) this;
    assert(((void *) obj == &(obj->head_.ref_count_)) && "RefCount must be FIRST field in ArObject structure!");
    return (RCObject) obj;
}

SideTable *RefCount::AllocOrGetSideTable() {
    RefBits current = this->bits_.load(std::memory_order_consume);
    SideTable *side;

    assert(!current.IsStatic());

    if (!current.IsInlineCounter())
        return current.GetSideTable();

    if ((side = (SideTable *) Alloc(sizeof(SideTable))) == nullptr)
        return nullptr;

    side->strong.store(current.GetStrong());
    side->weak.store(1);
    side->object = this->GetObjectBase();

    RefBits desired((uintptr_t) side);

    if (current.IsGcObject())
        desired.SetGCBit();

    do {
        if (!current.IsInlineCounter()) {
            Free(side);
            side = current.GetSideTable();
            break;
        }

        side->strong.store(current.GetStrong());

    } while (!this->bits_.compare_exchange_weak(current, desired, std::memory_order_release,
                                                std::memory_order_relaxed));
    return side;
}

bool RefCount::DecStrong() {
    RefBits current = this->bits_.load(std::memory_order_consume);
    RefBits desired = {};
    bool release;

    if (current.IsStatic())
        return false;

    do {
        desired = current;

        if (!desired.IsInlineCounter()) {
            auto side = desired.GetSideTable();

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

        release = desired.Decrement();
    } while (!this->bits_.compare_exchange_weak(current, desired, std::memory_order_relaxed));

    return release;
}

bool RefCount::DecWeak() const {
    RefBits current = this->bits_.load(std::memory_order_consume);
    assert(!current.IsInlineCounter());

    auto side = current.GetSideTable();
    auto weak = side->weak.fetch_sub(1);

    if (weak == 1)
        Free(side);

    return weak <= 2;
}

bool RefCount::HaveSideTable() const {
    RefBits current = this->bits_.load(std::memory_order_relaxed);
    return !current.IsStatic() && !current.IsInlineCounter();
}

bool RefCount::IncStrong() {
    RefBits current = this->bits_.load(std::memory_order_consume);
    RefBits desired = {};

    if (current.IsStatic())
        return true;

    do {
        desired = current;

        if (!desired.IsInlineCounter()) {
            assert(desired.GetSideTable()->strong.fetch_add(1) != 0);
            return true;
        }

        assert(desired.GetStrong() > 0);

        if (desired.Increment()) {
            // Inline counter overflow
            auto *side = this->AllocOrGetSideTable();
            if (side == nullptr)
                return false;

            side->strong++;
            return true;
        }
    } while (!this->bits_.compare_exchange_weak(current, desired, std::memory_order_relaxed));

    return true;
}

RefBits RefCount::IncWeak() {
    auto side = this->AllocOrGetSideTable();
    if (side != nullptr) {
        side->weak++;
        return RefBits((uintptr_t) side);
    }

    return RefBits(0);
}

uintptr_t RefCount::GetStrongCount() const {
    RefBits current = this->bits_.load(std::memory_order_consume);

    if (current.IsInlineCounter() || current.IsStatic())
        return current.GetStrong();

    return current.GetSideTable()->strong;
}

uintptr_t RefCount::GetWeakCount() const {
    RefBits current = this->bits_.load(std::memory_order_consume);

    if (!current.IsStatic() && !current.IsInlineCounter())
        return current.GetSideTable()->weak;

    return 0;
}