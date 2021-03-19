// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>
#include <vm/runtime.h>

#include "error.h"
#include "integer.h"
#include "iterator.h"
#include "list.h"
#include "option.h"
#include "bounds.h"
#include "bool.h"

using namespace argon::object;
using namespace argon::memory;

size_t list_len(ArObject *obj) {
    return ((List *) obj)->len;
}

ArObject *argon::object::ListGetItem(List *self, ArSSize index) {
    if (index < 0)
        index = self->len + index;

    if (index < self->len)
        return IncRef(self->objects[index]);

    return ErrorFormat(&error_overflow_error, "list index out of range (len: %d, idx: %d)", self->len, index);
}

bool argon::object::ListSetItem(List *self, ArObject *obj, ArSSize index) {
    if (index < 0)
        index = self->len + index;

    if (index < self->len) {
        Release(self->objects[index]);
        self->objects[index] = IncRef(obj);
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

    ret->len = slice_len;

    return ret;
}

bool list_set_slice(List *self, Bounds *bounds, ArObject *obj) {
    return false;
}

const SequenceSlots list_actions{
        list_len,
        (BinaryOpArSize) argon::object::ListGetItem,
        (BoolTernOpArSize) argon::object::ListSetItem,
        (BinaryOp) list_get_slice,
        (BoolTernOp) list_set_slice
};

bool CheckSize(List *list, size_t count) {
    ArObject **tmp;
    size_t len = count > 1 ? list->cap + count : (list->cap + 1) + ((list->cap + 1) / 2);

    if (list->len + count > list->cap) {
        if (list->objects == nullptr)
            len = ARGON_OBJECT_LIST_INITIAL_CAP;

        if ((tmp = (ArObject **) Realloc(list->objects, len * sizeof(void *))) == nullptr) {
            argon::vm::Panic(OutOfMemoryError);
            return false;
        }

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

ARGON_METHOD5(list_, append,
              "Add an item to the end of the list."
              ""
              "- Parameter obj: object to insert."
              "- Returns: list itself.", 1, false) {
    if (!ListAppend((List *) self, *argv))
        return nullptr;

    return IncRef(self);
}

ARGON_METHOD5(list_, clear,
              "Remove all items from the list."
              ""
              "- Returns: list itself.", 0, false) {
    ListClear((List *) self);
    return IncRef(self);
}

ARGON_METHOD5(list_, extend,
              "Extend the list by appending all the items from the iterable."
              ""
              "- Parameter iterable: an iterable object."
              "- Returns: list itself."
              "- Panic TypeError: object is not iterable.", 1, false) {
    ArObject *iter;
    ArObject *obj;

    if ((iter = IteratorGet(*argv)) == nullptr)
        return nullptr;

    while ((obj = IteratorNext(iter)) != nullptr) {
        if (!ListAppend((List *) self, obj)) {
            Release(obj);
            Release(iter);
            return nullptr;
        }
        Release(obj);
    }

    Release(iter);
    return IncRef(self);
}

ARGON_METHOD5(list_, find,
              "Find an item into the list and returns its position."
              ""
              "- Parameter obj: object to search."
              "- Returns: index if the object was found into the list, -1 otherwise.", 1, false) {
    auto *list = (List *) self;

    for (ArSize i = 0; i < list->len; i++) {
        if (AR_EQUAL(list->objects[i], *argv))
            return IntegerNew(i);
    }

    return IntegerNew(-1);
}

ARGON_METHOD5(list_, insert,
              "Insert an item at a given position."
              ""
              "- Parameters:"
              "  - index: index of the element before which to insert."
              "  - obj: object to insert."
              "- Returns: list itself."
              "- Panic TypeError: 'index' cannot be interpreted as an integer.", 2, false) {
    auto *list = (List *) self;
    ArSize idx;

    if (!AsNumber(argv[0]) || AR_NUMBER_SLOT(argv[0])->as_index == nullptr)
        return ErrorFormat(&error_type_error, "'%s' cannot be interpreted as an integer", AR_TYPE_NAME(argv[0]));

    idx = idx = AR_NUMBER_SLOT(argv[0])->as_index(argv[0]);

    if (idx > list->len)
        return ARGON_CALL_FUNC5(list_, append, list, argv + 1, 1);

    if (!ListSetItem(list, argv[1], idx))
        return nullptr;

    return IncRef(list);
}

ARGON_METHOD5(list_, pop,
              "Remove and returns the item at the end of the list."
              ""
              "- Returns: Option<?>", 0, false) {
    auto *list = (List *) self;
    ArObject *obj;

    if (list->len > 0) {
        obj = OptionNew(list->objects[list->len - 1]);
        Release(list->objects[--list->len]);
        return obj;
    }

    return OptionNew();
}

ARGON_METHOD5(list_, remove,
              "Remove the first item from the list whose value is equal to obj."
              ""
              "- Parameter obj: object to delete."
              "- Returns: true if obj was found and deleted, false otherwise.", 1, false) {
    auto *list = (List *) self;
    ArObject *ret = False;

    for (ArSize i = 0; i < list->len; i++) {
        if (AR_EQUAL(list->objects[i], argv[0])) {
            Release(list->objects[i]);
            list->len--;

            // move objects back
            for (ArSize j = i; j < list->len; j++)
                list->objects[j] = list->objects[j + 1];

            ret = True;
            break;
        }
    }

    return IncRef(ret);
}

ARGON_METHOD5(list_, reverse,
              "Reverse the elements of the list in place."
              ""
              "- Returns: list itself.", 0, false) {
    auto *list = (List *) self;
    ArSize si = list->len - 1;
    ArSize li = 0;

    ArObject *tmp;

    if (list->len > 1) {
        while (li < si) {
            tmp = list->objects[li];
            list->objects[li++] = list->objects[si];
            list->objects[si--] = tmp;
        }
    }

    return IncRef(self);
}

const NativeFunc list_methods[] = {
        list_append_,
        list_clear_,
        list_extend_,
        list_find_,
        list_insert_,
        list_pop_,
        list_remove_,
        list_reverse_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots list_obj = {
        list_methods,
        nullptr,
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
    int rec;

    if ((rec = TrackRecursive(self)) != 0)
        return rec > 0 ? StringIntern("[...]") : nullptr;


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

    UntrackRecursive(self);
    return StringBuilderFinish(&sb);

    error:
    Release(tmp);
    StringBuilderClean(&sb);
    UntrackRecursive(self);
    return nullptr;
}

ArObject *list_iter_get(List *self) {
    return IteratorNew(self, false);
}

ArObject *list_iter_rget(List *self) {
    return IteratorNew(self, true);
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
        (UnaryOp) list_iter_get,
        (UnaryOp) list_iter_rget,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &list_obj,
        &list_actions,
        &list_ops,
        nullptr
};

template<typename T>
List *ListClone(T *t) {
    // Simple hack to avoid to write two identical function (one for List and another for Tuple)
    // List and Tuple structures have the same field names.
    List *list;

    if ((list = ListNew(t->len)) == nullptr)
        return nullptr;

    for (size_t i = 0; i < t->len; i++)
        list->objects[i] = IncRef(t->objects[i]);

    list->len = t->len;
    return list;
}

template<typename T>
bool ListConcat(List *base, T *t) {
    if (!CheckSize(base, t->len))
        return false;

    for (size_t i = 0; i < t->len; i++)
        base->objects[i] = IncRef(t->objects[i]);

    base->len += t->len;
    return true;
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
    List *list;
    ArObject *ret;
    ArSize idx = 0;

    if (AsSequence(sequence)) {
        // FAST PATH
        if (AR_TYPEOF(sequence, type_list_))
            return ListClone((List *) sequence);
        else if (AR_TYPEOF(sequence, type_tuple_))
            return ListClone((Tuple *) sequence);

        // Generic sequence
        if ((list = ListNew((size_t) AR_SEQUENCE_SLOT(sequence)->length((ArObject *) sequence))) == nullptr)
            return nullptr;

        while (idx < list->cap) {
            ret = AR_SEQUENCE_SLOT(sequence)->get_item((ArObject *) sequence, idx++);
            ListAppend(list, ret);
            Release(ret);
        }

        return list;
    }

    return (List *) ErrorFormat(&error_not_implemented, "no viable conversion from '%s' to list",
                                AR_TYPE_NAME(sequence));
}

bool argon::object::ListAppend(List *list, ArObject *obj) {
    if (!CheckSize(list, 1))
        return false;

    list->objects[list->len++] = IncRef(obj);
    return true;
}

bool argon::object::ListConcat(List *list, ArObject *sequence) {
    ArObject *tmp;
    ArObject *iter;

    if (AR_SAME_TYPE(list, sequence))
        return ::ListConcat < List > (list, (List *) sequence);
    else if (AR_TYPEOF(sequence, type_tuple_))
        return ::ListConcat < Tuple > (list, (Tuple *) sequence);

    if ((iter = IteratorGet(sequence)) == nullptr)
        return false;

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (!ListAppend(list, tmp)) {
            Release(tmp);
            Release(iter);
            return false;
        }
        Release(tmp);
    }

    Release(iter);
    return true;
}

void argon::object::ListClear(List *list) {
    if (list->len == 0)
        return;

    while (list->len) {
        list->len--;
        Release(list->objects[list->len]);
    }
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
