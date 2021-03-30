// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bool.h"
#include "error.h"
#include "string.h"
#include "iterator.h"

using namespace argon::object;

bool iterator_has_next(Iterator *self) {
    if (self->reversed)
        return self->index > 0;

    return self->index < AR_SEQUENCE_SLOT(self->obj)->length(self->obj);
}

ArObject *iterator_next(Iterator *self) {
    ArObject *ret;

    if (!iterator_has_next(self))
        return nullptr;

    if (!self->reversed) {
        if ((ret = AR_SEQUENCE_SLOT(self->obj)->get_item(self->obj, self->index)) != nullptr)
            self->index++;
    } else {
        if ((ret = AR_SEQUENCE_SLOT(self->obj)->get_item(self->obj, self->index - 1)) != nullptr)
            self->index--;
    }


    return ret;
}

ArObject *iterator_peek(Iterator *self) {
    auto idx = self->index;
    auto ret = iterator_next(self);

    self->index = idx;

    return ret;
}

ArObject *argon::object::IteratorStr(Iterator *iterator) {
    if (AR_TYPEOF(iterator, type_iterator_))
        return StringNewFormat("<%s iterator @%p>", AR_TYPE_NAME(iterator->obj), iterator);

    return StringNewFormat("<%s @%p>", AR_TYPE_NAME(iterator), iterator);
}

const IteratorSlots iterator_slots = {
        (BoolUnaryOp) iterator_has_next,
        (UnaryOp) iterator_next,
        (UnaryOp) iterator_peek,
        (VoidUnaryOp) IteratorReset
};

bool argon::object::IteratorEqual(Iterator *iterator, ArObject *other) {
    auto *o = (Iterator *) other;

    if (iterator == other)
        return true;

    if (!AR_SAME_TYPE(iterator, other))
        return false;

    return iterator->reversed == o->reversed
           && iterator->index == o->index
           && AR_EQUAL(iterator->obj, o->obj);
}

ArObject *iterator_compare(Iterator *iterator, ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(iterator, other) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(IteratorEqual(iterator, other));
}

const TypeInfo argon::object::type_iterator_ = {
        TYPEINFO_STATIC_INIT,
        "iterator",
        nullptr,
        sizeof(Iterator),
        nullptr,
        (VoidUnaryOp) IteratorCleanup,
        nullptr,
        (CompareOp) iterator_compare,
        (BoolBinOp) IteratorEqual,
        (BoolUnaryOp) iterator_has_next,
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
        nullptr
};

Iterator *argon::object::IteratorNew(const TypeInfo *type, ArObject *iterable, bool reversed) {
    Iterator *iter;

    if (!AsSequence(iterable))
        return (Iterator *) ErrorFormat(&error_type_error,
                                        "unable to create a generic iterator for '%s' object who not implement SequenceSlots",
                                        AR_TYPE_NAME(iterable));

    if ((iter = ArObjectNew<Iterator>(RCType::INLINE, type)) != nullptr) {
        iter->obj = IncRef(iterable);

        iter->index = 0;

        if (reversed)
            iter->index = AR_SEQUENCE_SLOT(iterable)->length(iterable);

        iter->reversed = reversed;
    }

    return iter;
}

void argon::object::IteratorReset(Iterator *iterator) {
    if (iterator->reversed) {
        iterator->index = AR_SEQUENCE_SLOT(iterator->obj)->length(iterator->obj);
        return;
    }

    iterator->index = 0;
}