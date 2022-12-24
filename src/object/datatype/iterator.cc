// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bool.h"
#include "error.h"
#include "string.h"
#include "iterator.h"

using namespace argon::object;

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
    return StringNewFormat("<%s @%p>", AR_TYPE_NAME(iterator), iterator);
}

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
        iter->reversed = reversed;
    }

    return iter;
}