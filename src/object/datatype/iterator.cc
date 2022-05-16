// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "bool.h"
#include "error.h"
#include "string.h"
#include "iterator.h"

using namespace argon::object;

ArObject *iterator_next(Iterator *self) {
    UniqueLock lock(self->lock);
    ArObject *ret;

    if (!self->reversed) {
        if ((ret = AR_SEQUENCE_SLOT(self->obj)->get_item(self->obj, self->index)) != nullptr)
            self->index++;
    } else {
        if (self->index == 0)
            return nullptr;

        if ((ret = AR_SEQUENCE_SLOT(self->obj)->get_item(self->obj, self->index - 1)) != nullptr)
            self->index--;
    }

    lock.RelinquishLock();

    argon::vm::DiscardErrorType(type_overflow_error_);
    return ret;
}

ArObject *iterator_peek(Iterator *self) {
    UniqueLock lock(self->lock);
    ArObject *ret;

    if (!self->reversed)
        ret = AR_SEQUENCE_SLOT(self->obj)->get_item(self->obj, self->index);
    else {
        if (self->index == 0)
            return nullptr;

        ret = AR_SEQUENCE_SLOT(self->obj)->get_item(self->obj, self->index - 1);
    }

    lock.RelinquishLock();

    argon::vm::DiscardErrorType(type_overflow_error_);
    return ret;
}

const IteratorSlots iterator_slots = {
        nullptr,
        (UnaryOp) iterator_next,
        (UnaryOp) iterator_peek,
        nullptr
};

bool iterator_is_true(ArObject *self){
    return true;
}

ArObject *argon::object::IteratorCompare(Iterator *self, ArObject *other, CompareMode mode) {
    auto *o = (Iterator *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    UniqueLock self_lock(self->lock);
    UniqueLock o_lock(o->lock);

    return BoolToArBool(self->reversed == o->reversed && self->index == o->index
                        && Equal(self->obj, o->obj));
}

ArObject *argon::object::IteratorStr(Iterator *iterator) {
    if (AR_TYPEOF(iterator, type_iterator_))
        return StringNewFormat("<%s iterator @%p>", AR_TYPE_NAME(iterator->obj), iterator);

    return StringNewFormat("<%s @%p>", AR_TYPE_NAME(iterator), iterator);
}

const TypeInfo IteratorType = {
        TYPEINFO_STATIC_INIT,
        "iterator",
        nullptr,
        sizeof(Iterator),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) IteratorCleanup,
        nullptr,
        (CompareOp) IteratorCompare,
        iterator_is_true,
        nullptr,
        nullptr,
        (UnaryOp) IteratorStr,
        nullptr,
        nullptr,
        nullptr,
        &iterator_slots,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_iterator_ = &IteratorType;

Iterator *argon::object::IteratorNew(const TypeInfo *type, ArObject *iterable, bool reversed) {
    Iterator *iter;

    if (!AsSequence(iterable))
        return (Iterator *) ErrorFormat(type_type_error_,
                                        "unable to create a generic iterator for '%s' object who not implement SequenceSlots",
                                        AR_TYPE_NAME(iterable));

    if ((iter = ArObjectNew<Iterator>(RCType::INLINE, type)) != nullptr) {
        iter->lock = false;

        iter->obj = IncRef(iterable);
        iter->index = 0;

        if (reversed)
            iter->index = AR_SEQUENCE_SLOT(iterable)->length(iterable);

        iter->reversed = reversed;
    }

    return iter;
}