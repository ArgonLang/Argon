// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <object/objmgmt.h>
#include "integer.h"
#include "list.h"

using namespace argon::object;
using namespace argon::memory;

ArObject *list_add(List *self, ArObject *other) {
    if (self->type == other->type) {
        auto o = (List *) other;
        auto res = ListNew(self->len + o->len);
        size_t i = 0;

        if (res != nullptr) {
            for (; i < self->len; i++) {
                IncRef(self->objects[i]);
                res->objects[i] = self->objects[i];
            }

            for (; i < self->len + o->len; i++) {
                IncRef(o->objects[i - self->len]);
                res->objects[i] = o->objects[i - self->len];
            }

            res->len = self->len + o->len;
        }

        return res;
    }

    return nullptr;
}

ArObject *list_mul(ArObject *self, ArObject *other) {
    auto list = (List *) self;

    if (self->type != &type_list_) {
        list = (List *) other;
        other = self;
    }

    if (other->type == &type_integer_) {
        auto res = ListNew(list->len * ((Integer *) other)->integer);

        if (res != nullptr) {
            for (size_t i = 0; i < res->cap; i++) {
                IncRef(list->objects[i % list->len]);
                res->objects[i] = list->objects[i % list->len];
            }

            res->len = res->cap;
        }

        return res;
    }

    return nullptr;
}

List *ShiftList(List *list, arsize pos) {
    auto ret = ListNew(list->len);

    if (ret != nullptr) {
        for (size_t i = 0; i < list->len; i++) {
            IncRef(list->objects[i]);
            ret->objects[((list->len + pos) + i) % list->len] = list->objects[i];
        }
        ret->len = list->len;
    }

    return ret;
}

ArObject *list_shl(ArObject *self, ArObject *other) {
    if (self->type == &type_list_ && other->type == &type_integer_)
        return ShiftList((List *) self, -((Integer *) other)->integer);
    return nullptr;
}

ArObject *list_shr(ArObject *self, ArObject *other) {
    if (self->type == &type_list_ && other->type == &type_integer_)
        return ShiftList((List *) self, ((Integer *) other)->integer);
    return nullptr;
}

OpSlots list_ops{
        (BinaryOp) list_add,
        nullptr,
        list_mul,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        list_shl,
        list_shr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

bool list_equal(ArObject *self, ArObject *other) {
    if (self == other)
        return true;

    if (self->type == other->type) {
        auto l1 = (List *) self;
        auto l2 = (List *) other;

        if (l1->len != l2->len)
            return false;

        for (size_t i = 0; i < l1->len; i++)
            if (!l1->objects[i]->type->equal(l1->objects[i], l2->objects[i]))
                return false;

        return true;
    }

    return false;
}

size_t list_hash(ArObject *obj) {
    return 0;
}

size_t list_len(ArObject *obj) {
    return ((List *) obj)->len;
}

void list_cleanup(ArObject *obj) {
    auto list = (List *) obj;
    for (size_t i = 0; i < list->len; i++)
        Release(list->objects[i]);
}

void list_trace(List *self, VoidUnaryOp trace) {
    for (size_t i = 0; i < self->len; i++)
        trace(self->objects[i]);
}

const SequenceActions list_actions{
        list_len,
        (BinaryOpArSize) argon::object::ListGetItem,
};

const TypeInfo argon::object::type_list_ = {
        (const unsigned char *) "list",
        sizeof(List),
        nullptr,
        &list_actions,
        nullptr,
        nullptr,
        nullptr,
        list_equal,
        nullptr,
        list_hash,
        &list_ops,
        (Trace) list_trace,
        list_cleanup
};

ArObject *argon::object::ListGetItem(List *list, arsize i) {
    ArObject *obj;

    if (i >= list->len)
        return nullptr;

    obj = list->objects[i];
    IncRef(obj);
    return obj;
}

bool CheckSize(List *list, size_t count) {
    ArObject **tmp;

    if (list->len + count > list->cap) {

        if (list->objects != nullptr) {
            if (count > 1)
                tmp = (ArObject **) Realloc(list->objects, (list->cap + count) * sizeof(ArObject *));
            else
                tmp = (ArObject **) Realloc(list->objects, (list->cap + (list->cap / 2)) * sizeof(ArObject *));
        } else {
            if (count > 1)
                tmp = (ArObject **) Alloc(count * sizeof(ArObject *));
            else
                tmp = (ArObject **) Alloc(ARGON_OBJECT_LIST_INITIAL_CAP * sizeof(ArObject *));
        }


        if (tmp == nullptr)
            return false;

        list->objects = tmp;
        list->cap += list->cap / 2;
    }

    return true;
}

bool argon::object::ListAppend(List *list, ArObject *obj) {
    if (!CheckSize(list, 1)) {
        assert(false);
        return false;
    }
    IncRef(obj);
    list->objects[list->len] = obj;
    list->len++;
    return true;
}

bool argon::object::ListConcat(List *list, ArObject *sequence) {
    if (IsSequence(sequence)) {
        if (sequence->type == &type_list_) {
            auto other = (List *) sequence;

            if (!CheckSize(list, other->len))
                return false;

            for (size_t i = 0; i < other->len; i++) {
                IncRef(other->objects[i]);
                list->objects[list->len + i] = other->objects[i];
            }

            list->len += other->len;
            return true;
        }
    }
    assert(false); // TODO: impl
}

void argon::object::ListRemove(List *list, arsize i) {
    if (i >= list->len)
        return;

    Release(list->objects[i]);
    for (size_t idx = i + 1; idx < list->len; idx++)
        list->objects[idx - 1] = list->objects[idx];

    list->len--;
}

List *argon::object::ListNew() {
    return ListNew(ARGON_OBJECT_LIST_INITIAL_CAP);
}

List *argon::object::ListNew(size_t cap) {
    auto list = ArObjectNewGC<List>(&type_list_);

    if (list != nullptr) {
        list->objects = nullptr;

        if (cap > 0) {
            list->objects = (ArObject **) Alloc(cap * sizeof(ArObject *));
            if (list->objects == nullptr) {
                Release(list);
                return (List *) argon::vm::Panic(OutOfMemoryError);
            }
        }

        list->len = 0;
        list->cap = cap;
    }

    return list;
}

List *argon::object::ListNew(const ArObject *sequence) {
    List *list;
    ArObject *tmp;

    if (IsSequence(sequence)) {
        if (sequence->type == &type_list_) {
            // List clone
            auto other = (List *) sequence;

            if ((list = ListNew(other->len)) == nullptr)
                return nullptr;

            for (size_t i = 0; i < other->len; i++) {
                tmp = (ArObject *) other->objects[i];
                IncRef(tmp);
                list->objects[i] = tmp;
            }

            list->len = other->len;

            return list;
        }
    }

    return (List *) ErrorFormat(&error_not_implemented, "no viable conversion from '%s' to List", sequence->type->name);
}