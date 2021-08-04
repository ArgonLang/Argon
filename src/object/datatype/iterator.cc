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

    argon::vm::DiscardErrorType(type_overflow_error_);
    return ret;
}

ArObject *iterator_peek(Iterator *self) {
    auto idx = self->index;
    auto ret = iterator_next(self);

    self->index = idx;

    return ret;
}

const IteratorSlots iterator_slots = {
        nullptr,
        (UnaryOp) iterator_next,
        (UnaryOp) iterator_peek,
        (VoidUnaryOp) IteratorReset
};

bool iterator_is_true(ArObject *self){
    return true;
}

ArObject *argon::object::IteratorCompare(Iterator *iterator, ArObject *other, CompareMode mode) {
    auto *o = (Iterator *) other;

    if (!AR_SAME_TYPE(iterator, other) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(iterator->reversed == o->reversed && iterator->index == o->index
                        && Equal(iterator->obj, o->obj));
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