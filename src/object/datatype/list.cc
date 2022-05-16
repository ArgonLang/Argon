// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>
#include <vm/runtime.h>
#include <object/gc.h>

#include "error.h"
#include "integer.h"
#include "iterator.h"
#include "list.h"
#include "option.h"
#include "bounds.h"
#include "bool.h"

using namespace argon::object;
using namespace argon::memory;

ArSize list_len(ArObject *obj) {
    return ((List *) obj)->len;
}

ArObject *argon::object::ListGetItem(List *self, ArSSize index) {
    RWLockRead lock(self->lock);

    if (index < 0)
        index = self->len + index;

    if (index < self->len)
        return IncRef(self->objects[index]);

    return ErrorFormat(type_overflow_error_, "list index out of range (len: %d, idx: %d)", self->len, index);
}

bool argon::object::ListSetItem(List *self, ArObject *obj, ArSSize index) {
    RWLockWrite lock(self->lock);

    if (index < 0)
        index = self->len + index;

    if (index < self->len) {
        Release(self->objects[index]);
        self->objects[index] = IncRef(obj);
        TrackIf(self, obj);
        return true;
    }

    ErrorFormat(type_overflow_error_, "list index out of range (len: %d, idx: %d)", self->len, index);
    return false;
}

ArObject *list_get_slice(List *self, Bounds *bounds) {
    RWLockRead lock(self->lock);
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
        for (ArSize i = 0; start < stop; start += step) {
            tmp = self->objects[start];
            IncRef(tmp);
            ret->objects[i++] = tmp;
            TrackIf(ret, tmp);
        }
    } else {
        for (ArSize i = 0; stop < start; start += step) {
            tmp = self->objects[start];
            IncRef(tmp);
            ret->objects[i++] = tmp;
            TrackIf(ret, tmp);
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

bool CheckSize(List *list, ArSize count) {
    ArObject **tmp;
    ArSize len = count > 1 ? list->cap + count : (list->cap + 1) + ((list->cap + 1) / 2);

    if (list->len + count > list->cap) {
        if (list->objects == nullptr)
            len = ARGON_OBJECT_LIST_INITIAL_CAP;

        if ((tmp = (ArObject **) Realloc(list->objects, len * sizeof(void *))) == nullptr) {
            argon::vm::Panic(error_out_of_memory);
            return false;
        }

        list->objects = tmp;
        list->cap = len;
    }

    return true;
}

List *ShiftList(List *list, ArSSize pos) {
    RWLockRead lock(list->lock);

    auto ret = ListNew(list->len);

    if (ret != nullptr) {
        for (ArSize i = 0; i < list->len; i++) {
            IncRef(list->objects[i]);
            ret->objects[((list->len + pos) + i) % list->len] = list->objects[i];
        }
        ret->len = list->len;
    }

    if (GCIsTracking(list))
        Track(ret);

    return ret;
}

ArObject *list_add(ArObject *left, ArObject *right) {
    auto *l = (List *) left;
    auto *r = (List *) right;
    List *list = nullptr;

    if (AR_SAME_TYPE(l, r)) {
        RWLockRead l_lock(l->lock);
        RWLockRead r_lock(r->lock);
        if ((list = ListNew(l->len + r->len)) != nullptr) {
            ArSize i = 0;
            ArObject *itm;

            // copy from left (self)
            for (; i < l->len; i++) {
                itm = IncRef(l->objects[i]);
                list->objects[i] = itm;
                TrackIf(list, itm);
            }

            // copy from right (other)
            for (; i < l->len + r->len; i++) {
                itm = IncRef(r->objects[i - l->len]);
                list->objects[i] = itm;
                TrackIf(list, itm);
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
        RWLockRead lock(list->lock);
        if ((ret = ListNew(list->len * ((Integer *) num)->integer)) != nullptr) {
            for (ArSize i = 0; i < ret->cap; i++) {
                IncRef(list->objects[i % list->len]);
                ret->objects[i] = list->objects[i % list->len];
            }

            ret->len = ret->cap;

            if (GCIsTracking(list))
                Track(ret);
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

    if (!AR_TYPEOF(list, type_list_)) {
        list = (List *) right;
        num = (Integer *) left;
    }

    if (AR_TYPEOF(num, type_integer_)) {
        if (ListMul(list, num->integer))
            return IncRef(list);
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

ARGON_FUNCTION5(list_, new, "Creates an empty list or construct it from an iterable object."
                            ""
                            "- Parameter [iter]: iterable object."
                            "- Returns: new list.", 0, true) {
    if (!VariadicCheckPositional("list::new", count, 0, 1))
        return nullptr;

    if (count == 1)
        return ListNew(*argv);

    return ListNew();
}

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

    if (!ListConcat((List *) self, *argv))
        return nullptr;

    return IncRef(self);
}

ARGON_METHOD5(list_, find,
              "Find an item into the list and returns its position."
              ""
              "- Parameter obj: object to search."
              "- Returns: index if the object was found into the list, -1 otherwise.", 1, false) {
    RWLockRead lock(((List *) self)->lock);
    auto *list = (List *) self;

    for (ArSize i = 0; i < list->len; i++) {
        if (Equal(list->objects[i], *argv))
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
        return ErrorFormat(type_type_error_, "'%s' cannot be interpreted as an integer", AR_TYPE_NAME(argv[0]));

    idx = idx = AR_NUMBER_SLOT(argv[0])->as_index(argv[0]);

    if (!ListInsert(list, argv[1], idx))
        return nullptr;

    return IncRef(list);
}

ARGON_METHOD5(list_, pop,
              "Remove and returns the item at the end of the list."
              ""
              "- Returns: Option<?>", 0, false) {
    auto *list = (List *) self;
    ArObject *obj;

    RWLockWrite lock(list->lock);

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

    RWLockWrite lock(list->lock);

    for (ArSize i = 0; i < list->len; i++) {
        if (Equal(list->objects[i], argv[0])) {
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
    RWLockWrite lock(((List *) self)->lock);
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
        list_new_,
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
        nullptr,
        nullptr,
        nullptr,
        -1
};

bool list_is_true(List *self) {
    return self->len > 0;
}

ArObject *list_compare(List *self, ArObject *other, CompareMode mode) {
    auto *l2 = (List *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other) {
        RWLockRead self_lock(self->lock);
        RWLockRead l2_lock(l2->lock);

        if (self->len != l2->len)
            return BoolToArBool(false);

        for (ArSize i = 0; i < self->len; i++)
            if (!Equal(self->objects[i], l2->objects[i]))
                return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ArObject *list_str(List *self) {
    StringBuilder builder;
    int rec;

    if ((rec = TrackRecursive(self)) != 0)
        return rec > 0 ? StringIntern("[...]") : nullptr;

    RWLockRead lock(self->lock);

    builder.Write((const unsigned char *) "[", 1, self->len == 0 ? 1 : 256);

    for (ArSize i = 0; i < self->len; i++) {
        auto *tmp = (String *) ToRepr(self->objects[i]);
        if (tmp == nullptr) {
            UntrackRecursive(self);
            return nullptr;
        }

        if (!builder.Write(tmp, i + 1 < self->len ? (int) (self->len - i) + 2 : 1)) {
            Release(tmp);
            UntrackRecursive(self);
            return nullptr;
        }

        if (i + 1 < self->len)
            builder.Write((const unsigned char *) ", ", 2, 0);

        Release(tmp);
    }

    builder.Write((const unsigned char *) "]", 1, 0);

    UntrackRecursive(self);
    return builder.BuildString();
}

ArObject *list_iter_get(List *self) {
    return IteratorNew(self, false);
}

ArObject *list_iter_rget(List *self) {
    return IteratorNew(self, true);
}

void list_trace(List *self, VoidUnaryOp trace) {
    for (ArSize i = 0; i < self->len; i++)
        trace(self->objects[i]);
}

void list_cleanup(List *self) {
    for (ArSize i = 0; i < self->len; i++)
        Release(self->objects[i]);

    Free(self->objects);
}

const TypeInfo ListType = {
        TYPEINFO_STATIC_INIT,
        "list",
        nullptr,
        sizeof(List),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) list_cleanup,
        (Trace) list_trace,
        (CompareOp) list_compare,
        (BoolUnaryOp) list_is_true,
        nullptr,
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
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_list_ = &ListType;

template<typename T>
List *ListClone(T *t) {
    // Simple hack to avoid to write two identical function (one for List and another for Tuple)
    // List and Tuple structures have the same field names.
    ArObject *itm;
    List *list;

    if ((list = ListNew(t->len)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < t->len; i++) {
        itm = IncRef(t->objects[i]);
        list->objects[i] = itm;
        TrackIf(list, itm);
    }

    list->len = t->len;
    return list;
}

template<typename T>
bool ListConcat(List *base, T *t) {
    ArObject *itm;

    if (!CheckSize(base, t->len))
        return false;

    for (ArSize i = 0; i < t->len; i++) {
        itm = IncRef(t->objects[i]);
        base->objects[base->len++] = itm;
        TrackIf(base, itm);
    }

    return true;
}

bool argon::object::ListInsertAt(List *self, ArObject *obj, ArSSize index) {
    RWLockWrite lock(self->lock);

    if (index < 0)
        index = (ArSSize) (self->len + index);

    if (index < self->len) {
        if (!CheckSize(self, 1))
            return false;

        for (ArSize i = self->len; i > index; i--)
            self->objects[i] = self->objects[i - 1];

        self->objects[index] = IncRef(obj);
        self->len++;

        TrackIf(self, obj);
        return true;
    }

    ErrorFormat(type_overflow_error_, "list index out of range (len: %d, idx: %d)", self->len, index);
    return false;
}

List *argon::object::ListNew(ArSize cap) {
    auto list = ArObjectGCNew<List>(type_list_);

    if (list != nullptr) {
        list->lock = 0;
        list->objects = nullptr;

        if (cap > 0) {
            if ((list->objects = (ArObject **) Alloc(cap * sizeof(void *))) == nullptr) {
                Release(list);
                return (List *) argon::vm::Panic(error_out_of_memory);
            }
        }

        list->len = 0;
        list->cap = cap;
    }

    return list;
}

List *argon::object::ListNew(const ArObject *object) {
    List *list;

    if (AsSequence(object)) {
        // FAST PATH
        if (AR_TYPEOF(object, type_list_)) {
            RWLockRead lock(((List *) object)->lock);
            return ListClone((List *) object);
        } else if (AR_TYPEOF(object, type_tuple_))
            return ListClone((Tuple *) object);
    }

    if (IsIterable(object)) {
        if ((list = ListNew()) != nullptr) {
            if (ListConcat(list, object))
                return list;

            Release(list);
        }

        return nullptr;
    }

    return (List *) ErrorFormat(type_not_implemented_, "no viable conversion from '%s' to list",
                                AR_TYPE_NAME(object));
}

bool argon::object::ListAppend(List *list, ArObject *obj) {
    RWLockWrite lock(list->lock);

    if (!CheckSize(list, 1))
        return false;

    list->objects[list->len++] = IncRef(obj);
    TrackIf(list, obj);
    return true;
}

bool argon::object::ListConcat(List *list, const ArObject *sequence) {
    ArObject *tmp;
    ArObject *iter;

    if (list == sequence)
        return ListMul(list, 2);

    if (AR_SAME_TYPE(list, sequence)) {
        RWLockWrite lock(list->lock);
        RWLockRead seq_lock(((List *) sequence)->lock);
        return ::ListConcat < List > (list, (List *) sequence);
    } else if (AR_TYPEOF(sequence, type_tuple_)) {
        RWLockWrite lock(list->lock);
        return ::ListConcat < Tuple > (list, (Tuple *) sequence);
    }

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

bool argon::object::ListInsert(List *list, ArObject *object, ArSSize index) {
    RWLockWrite lock(list->lock);

    if (index < 0)
        index = list->len + index;

    if (index > list->len) {
        if (!CheckSize(list, 1))
            return false;

        list->objects[list->len++] = IncRef(object);
        TrackIf(list, object);
        return true;
    }

    Release(list->objects[index]);
    list->objects[index] = IncRef(object);
    TrackIf(list, object);
    return true;
}

bool argon::object::ListMul(List *list, ArSSize n) {
    RWLockWrite lock(list->lock);
    ArSize nlen = list->len * n;

    if (n == 1)
        return true;

    if (!CheckSize(list, nlen))
        return false;

    for (ArSize i = list->len; i < nlen; i++)
        list->objects[i] = IncRef(list->objects[i % list->len]);

    list->len = nlen;

    IncRef(list);
    return list;
}

void argon::object::ListClear(List *list) {
    RWLockWrite lock(list->lock);

    if (list->len == 0)
        return;

    while (list->len) {
        list->len--;
        Release(list->objects[list->len]);
    }
}

void argon::object::ListRemove(List *list, ArSSize i) {
    RWLockWrite lock(list->lock);

    if (i >= list->len)
        return;

    Release(list->objects[i]);

    // Move items back
    for (ArSize idx = i + 1; idx < list->len; idx++)
        list->objects[idx - 1] = list->objects[idx];

    list->len--;
}
