// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>
#include <vm/runtime.h>

#include "error.h"
#include "integer.h"
#include "list.h"
#include "bounds.h"

using namespace argon::object;
using namespace argon::memory;

size_t list_len(ArObject *obj) {
    return ((List *) obj)->len;
}

ArObject *argon::object::ListGetItem(List *self, ArSSize index) {
    ArObject *obj;

    if (index < 0)
        index = self->len + index;

    if (index < self->len) {
        obj = self->objects[index];
        IncRef(obj);
        return obj;
    }

    return ErrorFormat(&error_overflow_error, "list index out of range (len: %d, idx: %d)", self->len, index);
}

bool list_set_item(List *self, ArObject *obj, ArSSize index) {
    if (index < self->len) {
        Release(self->objects[index]);
        IncRef(obj);
        self->objects[index] = obj;
        return true;
    }

    ErrorFormat(&error_overflow_error, "list index out of range (len: %d, idx: %d)", self->len, index);
    return false;
}

ArObject *list_get_slice(List *self, Bounds *bounds) {
    ArObject *tmp;
    List *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    slice_len = BoundsIndex(bounds, self->len, &start, &stop, &step);

    if ((ret = ListNew(slice_len)) == nullptr)
        return nullptr;

    if (step >= 0) {
        for (size_t i = 0; start < stop; start += step) {
            tmp = self->objects[start];
            IncRef(tmp);
            ret->objects[i++] = tmp;
        }
    } else {
        for (size_t i = 0; stop < start; start += step) {
            tmp = self->objects[start];
            IncRef(tmp);
            ret->objects[i++] = tmp;
        }
    }

    self->len = slice_len;

    return ret;
}

bool list_set_slice(List *self, Bounds *bounds, ArObject *obj) {
    return false;
}

const SequenceSlots list_actions{
        list_len,
        (BinaryOpArSize) argon::object::ListGetItem,
        (BoolTernOpArSize) list_set_item,
        (BinaryOp) list_get_slice,
        (BoolTernOp) list_set_slice
};

bool CheckSize(List *list, size_t count) {
    ArObject **tmp;
    size_t len = count > 1 ? list->cap + count : list->cap + (list->cap / 2);

    if (list->len + count > list->cap) {
        if (list->objects == nullptr)
            len = ARGON_OBJECT_LIST_INITIAL_CAP;

        if ((tmp = (ArObject **) Realloc(list->objects, len * sizeof(void *))) == nullptr)
            return false;

        list->objects = tmp;
        list->cap = len;
    }

    return true;
}

List *ShiftList(List *list, ArSSize pos) {
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

ArObject *list_add(ArObject *left, ArObject *right) {
    auto *l = (List *) left;
    auto *r = (List *) right;
    List *list = nullptr;

    if (AR_SAME_TYPE(l, r)) {
        if ((list = ListNew(l->len + r->len)) != nullptr) {
            size_t i = 0;

            // copy from left (self)
            for (; i < l->len; i++) {
                IncRef(l->objects[i]);
                list->objects[i] = l->objects[i];
            }

            // copy from right (other)
            for (; i < l->len + r->len; i++) {
                IncRef(r->objects[i - l->len]);
                list->objects[i] = r->objects[i - l->len];
            }

            list->len = l->len + r->len;
        }
    }

    return list;
}

ArObject *list_mul(ArObject *left, ArObject *right) {
    auto *list = (List *) left;
    auto *num = (Integer *) right;
    List *ret = nullptr;

    if (!AR_TYPEOF(list, type_list_)) {
        list = (List *) right;
        num = (Integer *) left;
    }

    if (AR_TYPEOF(num, type_integer_)) {
        if ((ret = ListNew(list->len * ((Integer *) num)->integer)) != nullptr) {
            for (size_t i = 0; i < ret->cap; i++) {
                IncRef(list->objects[i % list->len]);
                ret->objects[i] = list->objects[i % list->len];
            }

            ret->len = ret->cap;
        }
    }

    return ret;
}

ArObject *list_shl(ArObject *left, ArObject *right) {
    if (AR_TYPEOF(left, type_list_) && AR_TYPEOF(right, type_integer_))
        return ShiftList((List *) left, -((Integer *) right)->integer);

    return nullptr;
}

ArObject *list_shr(ArObject *left, ArObject *right) {
    if (AR_TYPEOF(left, type_list_) && AR_TYPEOF(right, type_integer_))
        return ShiftList((List *) left, ((Integer *) right)->integer);

    return nullptr;
}

ArObject *list_inp_add(ArObject *left, ArObject *right) {
    if (AR_SAME_TYPE(left, right)) {
        if (ListConcat((List *) left, right)) {
            IncRef(left);
            return left;
        }
    }

    return nullptr;
}

ArObject *list_inp_mul(ArObject *left, ArObject *right) {
    auto *list = (List *) left;
    auto *num = (Integer *) right;
    size_t nlen;

    if (!AR_TYPEOF(list, type_list_)) {
        list = (List *) right;
        num = (Integer *) left;
    }

    if (AR_TYPEOF(num, type_integer_)) {
        nlen = list->len * (num->integer - 1);

        if (!CheckSize(list, nlen))
            return nullptr;

        for (size_t i = list->len; i < nlen; i++) {
            IncRef(list->objects[i % list->len]);
            list->objects[i] = list->objects[i % list->len];
        }

        list->len += nlen;

        IncRef(list);
        return list;
    }

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
        list_inp_add,
        nullptr,
        list_inp_mul,
        nullptr,
        nullptr,
        nullptr
};

ArObject *list_ctor(ArObject **args, ArSize count) {
    if (!VariadicCheckPositional("list", count, 0, 1))
        return nullptr;

    if (count == 1)
        return ListNew(*args);

    return ListNew();
}

bool list_is_true(List *self) {
    return self->len > 0;
}

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

ArObject *list_str(List *self) {
    StringBuilder sb = {};
    String *tmp = nullptr;

    // TODO: check for infinite recursion

    if (StringBuilderWrite(&sb, (unsigned char *) "[", 1, self->len == 0 ? 1 : 0) < 0)
        goto error;

    for (size_t i = 0; i < self->len; i++) {
        if ((tmp = (String *) ToString(self->objects[i])) == nullptr)
            goto error;

        if (StringBuilderWrite(&sb, tmp, i + 1 < self->len ? 2 : 1) < 0)
            goto error;

        if (i + 1 < self->len) {
            if (StringBuilderWrite(&sb, (unsigned char *) ", ", 2) < 0)
                goto error;
        }

        Release(tmp);
    }

    if (StringBuilderWrite(&sb, (unsigned char *) "]", 1) < 0)
        goto error;

    return StringBuilderFinish(&sb);

    error:
    Release(tmp);
    StringBuilderClean(&sb);
    return nullptr;
}

void list_trace(List *self, VoidUnaryOp trace) {
    for (size_t i = 0; i < self->len; i++)
        trace(self->objects[i]);
}

void list_cleanup(List *self) {
    for (size_t i = 0; i < self->len; i++)
        Release(self->objects[i]);

    Free(self->objects);
}

const TypeInfo argon::object::type_list_ = {
        TYPEINFO_STATIC_INIT,
        "list",
        nullptr,
        sizeof(List),
        list_ctor,
        (VoidUnaryOp) list_cleanup,
        (Trace) list_trace,
        nullptr,
        list_equal,
        (BoolUnaryOp) list_is_true,
        nullptr,
        (UnaryOp) list_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &list_actions,
        &list_ops
};

template<typename T>
List *ListClone(T *t) {
    // Simple hack to avoid to write two identical function (one for List and another for Tuple)
    // List and Tuple structures have the same field names.
    List *list;

    if ((list = ListNew(t->len)) == nullptr)
        return nullptr;

    for (size_t i = 0; i < t->len; i++) {
        IncRef(t->objects[i]);
        list->objects[i] = t->objects[i];
    }

    list->len = t->len;
    return list;
}

List *argon::object::ListNew(size_t cap) {
    auto list = ArObjectGCNew<List>(&type_list_);

    if (list != nullptr) {
        list->objects = nullptr;

        if (cap > 0) {
            if ((list->objects = (ArObject **) Alloc(cap * sizeof(void *))) == nullptr) {
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
    if (IsSequence(sequence)) {
        if (AR_TYPEOF(sequence, type_list_))
            return ListClone((List *) sequence);
        else if (AR_TYPEOF(sequence, type_tuple_))
            return ListClone((Tuple *) sequence);
    }

    ErrorFormat(&error_not_implemented, "no viable conversion from '%s' to list", AR_TYPE_NAME(sequence));
    return nullptr;
}

bool argon::object::ListAppend(List *list, ArObject *obj) {
    if (!CheckSize(list, 1))
        return false;

    IncRef(obj);
    list->objects[list->len++] = obj;
    return true;
}

bool argon::object::ListConcat(List *list, ArObject *sequence) {
    auto *o = (List *) sequence;

    if (AR_SAME_TYPE(list, sequence)) {
        if (!CheckSize(list, o->len))
            return false;

        for (size_t i = 0; i < o->len; i++) {
            IncRef(o->objects[i]);
            list->objects[list->len + i] = o->objects[i];
        }

        list->len += o->len;
        return true;
    }

    return false;
}

void argon::object::ListRemove(List *list, ArSSize i) {
    if (i >= list->len)
        return;

    Release(list->objects[i]);

    // Move items back
    for (size_t idx = i + 1; idx < list->len; idx++)
        list->objects[idx - 1] = list->objects[idx];

    list->len--;
}
