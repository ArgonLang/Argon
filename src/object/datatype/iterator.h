// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_ITERATOR_H_
#define ARGON_OBJECT_ITERATOR_H_

#include <object/arobject.h>

#define ITERATOR_NEW(name, ctor, cleanup, has_next, next, peek, reset)  \
const IteratorSlots name##_iterator {                                   \
        has_next,                                                       \
        next,                                                           \
        peek,                                                           \
        (VoidUnaryOp)(reset)                                            \
};                                                                      \
const TypeInfo type_##name##_ = {                                       \
        TYPEINFO_STATIC_INIT,                                           \
        #name,                                                          \
        nullptr,                                                        \
        sizeof(Iterator),                                               \
        ctor,                                                           \
        (VoidUnaryOp)(cleanup),                                         \
        nullptr,                                                        \
        nullptr,                                                        \
        (BoolBinOp) IteratorEqual,                                      \
        nullptr,                                                        \
        nullptr,                                                        \
        (UnaryOp) argon::object::IteratorStr,                           \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        &name##_iterator,                                               \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr                                                         \
}

#define ITERATOR_NEW_DEFAULT(name, has_next, next, peek) \
ITERATOR_NEW(name, nullptr, argon::object::IteratorCleanup, has_next, next, peek, argon::object::IteratorReset)

namespace argon::object {

    struct Iterator : ArObject {
        ArObject *obj;
        ArSize index;

        bool reversed;
    };

    extern const TypeInfo type_iterator_;

    Iterator *IteratorNew(const TypeInfo *type, ArObject *iterable, bool reversed);

    inline Iterator *IteratorNew(ArObject *iterable, bool reversed) {
        return IteratorNew(&type_iterator_, iterable, reversed);
    }

    bool IteratorEqual(Iterator *iterator, ArObject *other);

    inline void IteratorCleanup(Iterator *iterator) { Release(iterator->obj); }

    ArObject *IteratorStr(Iterator *iterator);

    void IteratorReset(Iterator *iterator);

} // namespace argon::object

#endif // !ARGON_OBJECT_ITERATOR_H_
