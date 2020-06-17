// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "object.h"
#include "refcount.h"
#include "nil.h"

using namespace argon::object;
using namespace argon::memory;


RefCount::RefCount(RefBits status) : bits_(status) {}

RefCount &RefCount::operator=(RefBits status) {
    this->bits_.store(status);
    return *this;
}

SideTable *RefCount::AllocOrGetSideTable() {
    RefBits current = this->bits_.load(std::memory_order_consume);
    SideTable *side;

    assert(!current.IsStatic());

    if (!current.IsInlineCounter())
        return current.GetSideTable();

    side = (SideTable *) Alloc(sizeof(SideTable));
    side->strong_.store(current.GetStrong());
    side->weak_.store(1);
    side->object = this->GetObjectBase();

    RefBits desired((uintptr_t) side);
    do {
        if (!current.IsInlineCounter()) {
            Free(side);
            side = current.GetSideTable();
            break;
        }
    } while (!this->bits_.compare_exchange_weak(current, desired, std::memory_order_release,
                                                std::memory_order_relaxed));
    return side;
}

ArObject *RefCount::GetObjectBase() {
    auto obj = (ArObject *) this;
    assert(((void *) obj == &(obj->ref_count)) && "RefCount must be FIRST field in ArObject structure!");
    return obj;
}

void RefCount::IncStrong() {
    RefBits current = this->bits_.load(std::memory_order_consume);
    RefBits desired = {};

    if (current.IsStatic())
        return;

    do {
        desired = current;

        if (!desired.IsInlineCounter()) {
            assert(desired.GetSideTable()->strong_.fetch_add(1) != 0);
            return;
        }

        assert(desired.GetStrong() > 0);

        if (desired.Increment()) {
            // Inline counter overflow
            this->AllocOrGetSideTable()->strong_++;
            return;
        }
    } while (!this->bits_.compare_exchange_weak(current, desired, std::memory_order_relaxed));
}

RefBits RefCount::IncWeak() {
    auto side = this->AllocOrGetSideTable();
    side->weak_++;
    return RefBits((uintptr_t) side);
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

            if (side->strong_.fetch_sub(1) == 1) {
                // ArObject can be destroyed
                if (side->weak_.fetch_sub(1) == 1) {
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

bool RefCount::DecWeak() {
    RefBits current = this->bits_.load(std::memory_order_consume);
    assert(!current.IsInlineCounter());

    auto side = current.GetSideTable();
    auto weak = side->weak_.fetch_sub(1);

    if (weak == 1)
        Free(side);

    return weak <= 2;
}

ArObject *RefCount::GetObject() {
    RefBits current = this->bits_.load(std::memory_order_consume);

    if (current.IsInlineCounter())
        return this->GetObjectBase();

    auto side = current.GetSideTable();
    if (side->strong_.fetch_add(1) == 0) {
        side->strong_--;
        IncRef(NilVal);
        return NilVal;
    }

    return side->object;
}