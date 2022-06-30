// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_ITERATOR_H_
#define ARGON_OBJECT_ITERATOR_H_

#include <object/arobject.h>
#include <object/rwlock.h>

#define ITERATOR_NEW(name, next, peek)                                  \
const IteratorSlots name {                                              \
        nullptr,                                                        \
        (UnaryOp) next,                                                 \
        (UnaryOp) peek,                                                 \
        nullptr                                                         \
};                                                                      \
const TypeInfo type_##name##_ = {                                       \
        TYPEINFO_STATIC_INIT,                                           \
        #name,                                                          \
        nullptr,                                                        \
        sizeof(Iterator),                                               \
        TypeInfoFlags::BASE,                                            \
        nullptr,                                                        \
        (VoidUnaryOp)IteratorCleanup,                                   \
        nullptr,                                                        \
        (CompareOp) IteratorCompare,                                    \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        (UnaryOp) argon::object::IteratorStr,                           \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        &name,                                                          \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr                                                         \
}

namespace argon::object {

    struct Iterator : ArObject {
        SimpleLock lock;

        ArObject *obj;
        ArSSize index;

        bool reversed;
    };

    ArObject *IteratorCompare(Iterator *self, ArObject *other, CompareMode mode);

    ArObject *IteratorStr(Iterator *iterator);

    Iterator *IteratorNew(const TypeInfo *type, ArObject *iterable, bool reversed);

    inline void IteratorCleanup(Iterator *iterator) { Release(iterator->obj); }

} // namespace argon::object

#endif // !ARGON_OBJECT_ITERATOR_H_
