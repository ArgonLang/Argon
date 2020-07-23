// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "object.h"
#include "refcount.h"
#include "nil.h"

using namespace argon::object;
using namespace argon::memory;


RefCount::RefCount(RefBits status) : bits_(status) {}

RefCount::RefCount(RCType status) : bits_(RefBits((unsigned char) status)) {}

RefCount &RefCount::operator=(RefBits status) {
    this->bits_.store(status);
    return *this;
}

ArObject *RefCount::GetObjectBase() {
    auto obj = (ArObject *) this;
    assert(((void *) obj == &(obj->ref_count)) && "RefCount must be FIRST field in ArObject structure!");
    return obj;
}

ArObject *RefCount::GetObject() {
    RefBits current = this->bits_.load(std::memory_order_consume);

    if (current.IsInlineCounter()) {
        this->IncStrong();
        return this->GetObjectBase();
    }

    auto side = current.GetSideTable();
    uintptr_t strong = side->strong.load(std::memory_order_consume);
    uintptr_t desired;

    do {
        desired = strong + 1;
        if (desired == 1 || side->object == nullptr)
            return ReturnNil();
    } while (side->strong.compare_exchange_weak(strong, desired, std::memory_order_relaxed));

    return side->object;
}

SideTable *RefCount::AllocOrGetSideTable() {
    RefBits current = this->bits_.load(std::memory_order_consume);
    SideTable *side;

    assert(!current.IsStatic());

    if (!current.IsInlineCounter())
        return current.GetSideTable();

    side = (SideTable *) Alloc(sizeof(SideTable));
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
    } while (!this->bits_.compare_exchange_weak(current, desired, std::memory_order_release,
                                                std::memory_order_relaxed));
    return side;
}

RefBits RefCount::IncWeak() {
    auto side = this->AllocOrGetSideTable();
    side->weak++;
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

bool RefCount::DecWeak() {
    RefBits current = this->bits_.load(std::memory_order_consume);
    assert(!current.IsInlineCounter());

    auto side = current.GetSideTable();
    auto weak = side->weak.fetch_sub(1);

    if (weak == 1)
        Free(side);

    return weak <= 2;
}

bool RefCount::IsGcObject() {
    RefBits current = this->bits_.load(std::memory_order_relaxed);
    return current.IsGcObject();
}

uintptr_t RefCount::GetStrongCount() {
    RefBits current = this->bits_.load(std::memory_order_consume);

    if (current.IsInlineCounter() || current.IsStatic())
        return current.GetStrong();

    return current.GetSideTable()->strong;
}

uintptr_t RefCount::GetWeakCount() {
    RefBits current = this->bits_.load(std::memory_order_consume);

    if (!current.IsInlineCounter())
        return current.GetSideTable()->weak;

    return 0;
}

void RefCount::ClearWeakRef() {
    RefBits current = this->bits_.load(std::memory_order_consume);

    if (!current.IsInlineCounter())
        current.GetSideTable()->object = nullptr;
}

void RefCount::IncStrong() {
    RefBits current = this->bits_.load(std::memory_order_consume);
    RefBits desired = {};

    if (current.IsStatic())
        return;

    do {
        desired = current;

        if (!desired.IsInlineCounter()) {
            assert(desired.GetSideTable()->strong.fetch_add(1) != 0);
            return;
        }

        assert(desired.GetStrong() > 0);

        if (desired.Increment()) {
            // Inline counter overflow
            this->AllocOrGetSideTable()->strong++;
            return;
        }
    } while (!this->bits_.compare_exchange_weak(current, desired, std::memory_order_relaxed));
}
