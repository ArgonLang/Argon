// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_ITERATOR_H_
#define ARGON_OBJECT_ITERATOR_H_

#include <object/arobject.h>

#define ITERATOR_NEW(name, ctor, cleanup, next, peek, reset)            \
const IteratorSlots name##_iterator {                                   \
        nullptr,                                                        \
        next,                                                           \
        peek,                                                           \
        (VoidUnaryOp)(reset)                                            \
};                                                                      \
const TypeInfo type_##name##_ = {                                       \
        TYPEINFO_STATIC_INIT,                                           \
        #name,                                                          \
        nullptr,                                                        \
        sizeof(Iterator),                                               \
        TypeInfoFlags::BASE,                                            \
        ctor,                                                           \
        (VoidUnaryOp)(cleanup),                                         \
        nullptr,                                                        \
        (CompareOp) IteratorCompare,                                    \
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
        nullptr,                                                        \
        nullptr                                                         \
}

#define ITERATOR_NEW_DEFAULT(name, next, peek) \
ITERATOR_NEW(name, nullptr, argon::object::IteratorCleanup, next, peek, argon::object::IteratorReset)

namespace argon::object {

    struct Iterator : ArObject {
        ArObject *obj;
        ArSSize index;

        bool reversed;
    };

    extern const TypeInfo *type_iterator_;

    Iterator *IteratorNew(const TypeInfo *type, ArObject *iterable, bool reversed);

    inline Iterator *IteratorNew(ArObject *iterable, bool reversed) {
        return IteratorNew(type_iterator_, iterable, reversed);
    }

    inline void IteratorCleanup(Iterator *iterator) { Release(iterator->obj); }

    ArObject *IteratorCompare(Iterator *iterator, ArObject *other, CompareMode mode);

    ArObject *IteratorStr(Iterator *iterator);

    void IteratorReset(Iterator *iterator);

} // namespace argon::object

#endif // !ARGON_OBJECT_ITERATOR_H_
