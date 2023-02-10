// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "boolean.h"
#include "bounds.h"
#include "option.h"
#include "stringbuilder.h"

#include "list.h"

using namespace argon::vm::datatype;

// Prototypes

List *ListFromIterable(List **, ArObject *);

List *ListShift(List *, ArSSize);

bool CheckSize(List *, ArSize);

ARGON_FUNCTION(list_list, List,
               "Creates an empty list or construct it from an iterable object.\n"
               "\n"
               "- Parameter iter: Iterable object.\n"
               "- Returns: New list.\n",
               nullptr, true, false) {
    if (!VariadicCheckPositional(list_list.name, (unsigned int) argc, 0, 1))
        return nullptr;

    if (argc == 1)
        return (ArObject *) ListNew(*args);

    return (ArObject *) ListNew();
}

ARGON_METHOD(list_append, append,
             "Add an item to the end of the list.\n"
             "\n"
             "- Parameter object: Object to insert.\n"
             "- Returns: List itself.\n",
             ": object", false, false) {
    if (!ListAppend((List *) _self, *args))
        return nullptr;

    return IncRef(_self);
}

ARGON_METHOD(list_clear, clear,
             "Remove all items from the list.\n"
             "\n"
             "- Returns: list itself.\n",
             nullptr, false, false) {
    ListClear((List *) _self);
    return IncRef(_self);
}

ARGON_METHOD(list_extend, extend,
             "Extend the list by appending all the items from the iterable.\n"
             "\n"
             "- Parameter iterable: An iterable object.\n"
             "- Returns: List itself.\n",
             ": iterable", false, false) {

    if (!ListExtend((List *) _self, *args))
        return nullptr;

    return IncRef(_self);
}

ARGON_METHOD(list_find, find,
             "Find an item into the list and returns its position.\n"
             "\n"
             "- Parameter object: Object to search.\n"
             "- Returns: Index if the object was found into the list, -1 otherwise.\n",
             ": object", false, false) {
    auto *self = (List *) _self;

    // *** WARNING ***
    // Why std::unique_lock? See vm/sync/rsm.h
    std::unique_lock _(self->rwlock);

    for (ArSize i = 0; i < self->length; i++) {
        if (Equal(self->objects[i], *args))
            return (ArObject *) IntNew((IntegerUnderlying) i);
    }

    return (ArObject *) IntNew(-1);
}

ARGON_METHOD(list_insert, insert,
             "Insert an item at a given position.\n"
             "\n"
             "- Parameters:\n"
             "  - index: Index of the element before which to insert.\n"
             "  - object: Object to insert.\n"
             "- Returns: List itself.\n",
             "iu: index, : object", false, false) {
    auto *self = (List *) _self;
    bool ok = false;

    if (AR_TYPEOF(*args, type_int_))
        ok = ListInsert(self, args[0], ((Integer *) args[1])->sint);
    else if (AR_TYPEOF(*args, type_uint_))
        ok = ListInsert(self, args[0], (ArSSize) ((Integer *) args[1])->uint);
    else
        ErrorFormat(kTypeError[0], "expected %s/%s, got '%s'", type_int_->name,
                    type_uint_->name, AR_TYPE_NAME(args[0]));

    return ok ? (ArObject *) IncRef(self) : nullptr;
}

ARGON_METHOD(list_pop, pop,
             "Remove and returns the item at the end of the list.\n"
             "\n"
             "- Returns: Result.\n",
             nullptr, false, false) {
    auto *self = (List *) _self;
    ArObject *obj;

    std::unique_lock _(self->rwlock);

    if (self->length > 0) {
        obj = (ArObject *) OptionNew(self->objects[self->length - 1]);

        Release(self->objects[--self->length]);

        return obj;
    }

    return (ArObject *) OptionNew();
}

ARGON_METHOD(list_remove, remove,
             "Remove the first item from the list whose value is equal to object.\n"
             "\n"
             "- Parameter object: Object to delete.\n"
             "- Returns: True if obj was found and deleted, false otherwise.\n",
             ": object", false, false) {
    auto *self = (List *) _self;

    std::unique_lock _(self->rwlock);

    for (ArSize i = 0; i < self->length; i++) {
        if (Equal(self->objects[i], args[0])) {
            Release(self->objects[i]);

            // Move items back
            for (ArSize j = i + 1; j < self->length; j++)
                self->objects[j - 1] = self->objects[j];

            return BoolToArBool(true);
        }
    }

    return BoolToArBool(false);
}

ARGON_METHOD(list_reverse, reverse,
             "Reverse the elements of the list in place.\n"
             "\n"
             "- Returns: List itself.\n",
             nullptr, false, false) {
    auto *self = (List *) _self;

    std::unique_lock _(self->rwlock);

    ArObject *tmp;

    ArSize si = self->length - 1;
    ArSize li = 0;

    if (self->length > 1) {
        while (li < si) {
            tmp = self->objects[li];
            self->objects[li++] = self->objects[si];
            self->objects[si--] = tmp;
        }
    }

    return IncRef(_self);
}

const FunctionDef list_methods[] = {
        list_list,

        list_append,
        list_clear,
        list_extend,
        list_find,
        list_insert,
        list_pop,
        list_remove,
        list_reverse,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots list_objslot = {
        list_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *list_get_item(List *self, ArObject *index) {
    if (AR_TYPEOF(index, type_int_))
        return ListGet(self, ((Integer *) index)->sint);
    else if (AR_TYPEOF(index, type_uint_))
        return ListGet(self, (ArSSize) ((Integer *) index)->uint);

    ErrorFormat(kTypeError[0], "expected %s/%s, got '%s'", type_int_->name,
                type_uint_->name, AR_TYPE_NAME(index));
    return nullptr;
}

ArObject *list_get_slice(List *self, ArObject *bounds) {
    auto *b = (Bounds *) bounds;
    List *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    std::shared_lock _(self->rwlock);

    slice_len = BoundsIndex(b, self->length, &start, &stop, &step);

    if ((ret = ListNew(slice_len)) == nullptr)
        return nullptr;

    if (step >= 0) {
        for (ArSize i = 0; start < stop; start += step)
            ret->objects[i++] = IncRef(self->objects[start]);
    } else {
        for (ArSize i = 0; stop < start; start += step)
            ret->objects[i++] = IncRef(self->objects[start]);
    }

    return (ArObject *) ret;
}

ArObject *list_item_in(List *self, const ArObject *key) {
    std::shared_lock _(self->rwlock);

    for (ArSize i = 0; i < self->length; i++) {
        if (Equal(self->objects[i], key))
            return BoolToArBool(true);
    }

    return BoolToArBool(false);
}

ArSize list_length(const Tuple *self) {
    return self->length;
}

bool list_set_item(List *self, ArObject *index, ArObject *value) {
    if (AR_TYPEOF(index, type_int_))
        return ListInsert(self, value, ((Integer *) index)->sint);
    else if (AR_TYPEOF(index, type_uint_))
        return ListInsert(self, value, (ArSSize) ((Integer *) index)->uint);

    ErrorFormat(kTypeError[0], "expected %s/%s, got '%s'", type_int_->name,
                type_uint_->name, AR_TYPE_NAME(index));

    return false;
}

const SubscriptSlots list_subscript = {
        (ArSize_UnaryOp) list_length,
        (BinaryOp) list_get_item,
        (Bool_TernaryOp) list_set_item,
        (BinaryOp) list_get_slice,
        nullptr,
        (BinaryOp) list_item_in
};

ArObject *list_add(ArObject *left, ArObject *right) {
    auto *l = (List *) left;
    auto *r = (List *) right;
    List *list = nullptr;

    ArSize size_new;

    if (AR_SAME_TYPE(l, r)) {
        std::shared_lock left_lock(l->rwlock);

        if (l != r)
            r->rwlock.lock_shared();

        size_new = l->length + r->length;

        if ((list = ListNew(size_new)) != nullptr) {
            ArSize i = 0;

            // copy from left (self)
            for (; i < l->length; i++)
                list->objects[i] = IncRef(l->objects[i]);

            // copy from right (other)
            for (; i < size_new; i++)
                list->objects[i] = IncRef(r->objects[i - l->length]);

            list->length = size_new;
        }
    }

    if (l != r)
        r->rwlock.unlock_shared();

    return (ArObject *) list;
}

ArObject *list_inp_add(ArObject *left, ArObject *right) {
    if (AR_SAME_TYPE(left, right) && ListExtend((List *) left, right))
        return IncRef(left);

    return nullptr;
}

ArObject *list_mul(ArObject *left, ArObject *right) {
    auto *list = (List *) left;
    const auto *num = (Integer *) right;
    List *ret = nullptr;

    // int * list -> list * int
    if (!AR_TYPEOF(list, type_list_)) {
        list = (List *) right;
        num = (Integer *) left;
    }

    if (AR_TYPEOF(num, type_uint_)) {
        std::shared_lock _(list->rwlock);

        if ((ret = ListNew(list->length * num->uint)) != nullptr) {
            for (ArSize i = 0; i < ret->capacity; i++)
                ret->objects[i] = IncRef(list->objects[i % list->length]);

            ret->length = ret->capacity;
        }
    }

    return (ArObject *) ret;
}

ArObject *list_shl(ArObject *left, const ArObject *right) {
    if (AR_TYPEOF(left, type_list_) && AR_TYPEOF(right, type_int_))
        return (ArObject *) ListShift((List *) left, -((const Integer *) right)->sint);

    return nullptr;
}

ArObject *list_shr(ArObject *left, const ArObject *right) {
    if (AR_TYPEOF(left, type_list_) && AR_TYPEOF(right, type_int_))
        return (ArObject *) ListShift((List *) left, ((const Integer *) right)->sint);

    return nullptr;
}

const OpSlots list_ops{
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
        (BinaryOp) list_shl,
        (BinaryOp) list_shr,
        nullptr,
        list_inp_add,
        nullptr,
        nullptr,
        nullptr
};

bool CheckSize(List *list, ArSize count) {
    ArSize len = list->capacity + count;
    ArObject **tmp;

    if (count == 0)
        len = (list->capacity + 1) + ((list->capacity + 1) / 2);

    if (list->length + count > list->capacity) {
        if (list->objects == nullptr)
            len = kListInitialCapacity;

        if ((tmp = (ArObject **) argon::vm::memory::Realloc(list->objects, len * sizeof(void *))) == nullptr)
            return false;

        list->objects = tmp;
        list->capacity = len;
    }

    return true;
}

ArObject *list_compare(List *self, ArObject *other, CompareMode mode) {
    auto *o = (List *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    // *** WARNING ***
    // Why std::unique_lock? See vm/sync/rsm.h
    std::unique_lock self_lock(self->rwlock);
    std::unique_lock other_lock(o->rwlock);

    if (self->length != o->length)
        return BoolToArBool(false);

    for (ArSize i = 0; i < self->length; i++) {
        if (!Equal(self->objects[i], o->objects[i]))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ArObject *list_iter(List *self, bool reverse) {
    auto *li = MakeGCObject<ListIterator>(type_list_iterator_, true);

    if (li != nullptr) {
        new(&li->lock)std::mutex;

        li->iterable = IncRef(self);
        li->index = 0;
        li->reverse = reverse;
    }

    return (ArObject *) li;
}

ArObject *list_repr(List *self) {
    StringBuilder builder;
    ArObject *ret;

    int rec;
    if ((rec = RecursionTrack((ArObject *) self)) != 0)
        return rec > 0 ? (ArObject *) StringIntern("[...]") : nullptr;

    std::shared_lock _(self->rwlock);

    builder.Write((const unsigned char *) "[", 1, self->length == 0 ? 1 : 256);

    for (ArSize i = 0; i < self->length; i++) {
        auto *tmp = (String *) Repr(self->objects[i]);

        if (tmp == nullptr) {
            RecursionUntrack((ArObject *) self);
            return nullptr;
        }

        if (!builder.Write(tmp, i + 1 < self->length ? (int) (self->length - i) + 2 : 1)) {
            Release(tmp);

            RecursionUntrack((ArObject *) self);

            return nullptr;
        }

        if (i + 1 < self->length)
            builder.Write((const unsigned char *) ", ", 2, 0);

        Release(tmp);
    }

    builder.Write((const unsigned char *) "]", 1, 0);

    RecursionUntrack((ArObject *) self);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

bool list_dtor(List *self) {
    for (ArSize i = 0; i < self->length; i++)
        Release(self->objects[i]);

    argon::vm::memory::Free(self->objects);

    self->rwlock.~RecursiveSharedMutex();

    return true;
}

bool list_is_true(List *self) {
    std::shared_lock _(self->rwlock);

    return self->length > 0;
}

void list_trace(List *self, Void_UnaryOp trace) {
    std::shared_lock _(self->rwlock);

    for (ArSize i = 0; i < self->length; i++)
        trace(self->objects[i]);
}

TypeInfo ListType = {
        AROBJ_HEAD_INIT_TYPE,
        "List",
        nullptr,
        nullptr,
        sizeof(List),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) list_dtor,
        (TraceOp) list_trace,
        nullptr,
        (Bool_UnaryOp) list_is_true,
        (CompareOp) list_compare,
        (UnaryConstOp) list_repr,
        nullptr,
        (UnaryBoolOp) list_iter,
        nullptr,
        nullptr,
        nullptr,
        &list_objslot,
        &list_subscript,
        &list_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_list_ = &ListType;

ArObject *argon::vm::datatype::ListGet(List *list, ArSSize index) {
    std::shared_lock _(list->rwlock);

    if (index < 0)
        index = (ArSSize) list->length + index;

    if (index >= 0 && index < list->length)
        return IncRef(list->objects[index]);

    ErrorFormat(kOverflowError[0], kOverflowError[1], type_tuple_->name, list->length, index);
    return nullptr;
}

bool argon::vm::datatype::ListAppend(List *list, ArObject *object) {
    std::unique_lock _(list->rwlock);

    if (!CheckSize(list, 1))
        return false;

    list->objects[list->length++] = IncRef(object);

    argon::vm::memory::TrackIf((ArObject *) list, object);

    return true;
}

bool argon::vm::datatype::ListExtend(List *list, ArObject *iterator) {
    if (iterator == nullptr)
        return true;

    if (!AR_TYPEOF(iterator, type_list_) && !AR_TYPEOF(iterator, type_tuple_))
        return ListFromIterable(&list, iterator) != nullptr;

    std::unique_lock _(list->rwlock);

    if (AR_TYPEOF(iterator, type_list_)) {
        // List fast-path
        auto *right = (List *) iterator;

        if (list != right)
            right->rwlock.lock_shared();

        if (!CheckSize(list, right->length)) {
            if (list != right)
                right->rwlock.unlock_shared();

            return false;
        }

        for (ArSize i = 0; i < right->length; i++) {
            auto *object = IncRef(right->objects[i]);

            list->objects[list->length + i] = object;

            argon::vm::memory::TrackIf((ArObject *) list, object);
        }

        list->length += right->length;

        if (list != right)
            right->rwlock.unlock_shared();

        return true;
    } else if (AR_TYPEOF(iterator, type_tuple_)) {
        // Tuple fast-path
        const auto *right = (const Tuple *) iterator;

        if (!CheckSize(list, right->length))
            return false;

        for (ArSize i = 0; i < right->length; i++) {
            auto *object = IncRef(right->objects[i]);

            list->objects[list->length + i] = object;

            argon::vm::memory::TrackIf((ArObject *) list, object);
        }

        list->length += right->length;

        return true;
    }

    assert(false);
}

bool argon::vm::datatype::ListExtend(List *list, ArObject **object, ArSize count) {
    std::unique_lock _(list->rwlock);

    if (!CheckSize(list, count))
        return false;

    for (ArSize i = 0; i < count; i++) {
        auto *obj = object[i];

        list->objects[list->length + i] = IncRef(obj);

        argon::vm::memory::TrackIf((ArObject *) list, obj);
    }

    list->length += count;

    return true;
}

bool argon::vm::datatype::ListInsert(List *list, ArObject *object, ArSSize index) {
    std::unique_lock _(list->rwlock);

    if (index < 0)
        index = ((ArSSize) list->length) + index;

    if (index > list->length) {
        if (!CheckSize(list, 1))
            return false;

        list->objects[list->length++] = IncRef(object);

        argon::vm::memory::TrackIf((ArObject *) list, object);

        return true;
    }

    Release(list->objects[index]);
    list->objects[index] = IncRef(object);

    argon::vm::memory::TrackIf((ArObject *) list, object);

    return true;
}

List *ListFromIterable(List **dest, ArObject *iterable) {
    ArObject *iter;
    ArObject *tmp;

    List *ret;

    if ((iter = IteratorGet(iterable, false)) == nullptr)
        return nullptr;

    if (dest == nullptr) {
        ret = ListNew();
        if (ret == nullptr) {
            Release(iter);
            return nullptr;
        }
    } else
        ret = *dest;

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (!ListAppend(ret, tmp)) {
            Release(tmp);
            Release(iter);

            if (dest == nullptr)
                Release(ret);

            return nullptr;
        }

        Release(tmp);
    }

    Release(iter);

    return ret;
}

List *argon::vm::datatype::ListNew(ArSize capacity) {
    auto *list = MakeGCObject<List>(type_list_, false);

    if (list != nullptr) {
        list->objects = nullptr;
        list->capacity = capacity;
        list->length = 0;

        if (capacity > 0) {
            list->objects = (ArObject **) argon::vm::memory::Alloc(capacity * sizeof(void *));
            if (list->objects == nullptr) {
                Release(list);
                return nullptr;
            }
        }

        new(&list->rwlock)sync::RecursiveSharedMutex();
    }

    return list;
}

List *argon::vm::datatype::ListNew(ArObject *iterable) {
    List *ret;

    if (!AR_TYPEOF(iterable, type_list_) && !AR_TYPEOF(iterable, type_tuple_))
        return ListFromIterable(nullptr, iterable);

    if (AR_TYPEOF(iterable, type_list_)) {
        // List fast-path
        auto *list = (List *) iterable;

        std::shared_lock _(list->rwlock);

        if ((ret = ListNew(list->length)) == nullptr)
            return nullptr;

        for (ArSize i = 0; i < list->length; i++)
            ret->objects[i] = IncRef(list->objects[i]);

        return ret;
    } else if (AR_TYPEOF(iterable, type_tuple_)) {
        // Tuple fast-path
        const auto *tuple = (Tuple *) iterable;

        if ((ret = ListNew(tuple->length)) == nullptr)
            return nullptr;

        for (ArSize i = 0; i < tuple->length; i++)
            ret->objects[i] = IncRef(tuple->objects[i]);

        return ret;
    }

    ErrorFormat(kTypeError[0], kTypeError[8], AR_TYPE_NAME(iterable), type_list_->name);
    return nullptr;
}

List *ListShift(List *list, ArSSize pos) {
    std::shared_lock lock(list->rwlock);

    auto ret = ListNew(list->length);
    if (ret != nullptr) {
        for (ArSize i = 0; i < list->length; i++)
            ret->objects[((list->length + pos) + i) % list->length] = IncRef(list->objects[i]);

        ret->length = list->length;
    }

    return ret;
}

void argon::vm::datatype::ListClear(List *list) {
    std::unique_lock _(list->rwlock);

    for (ArSize i = 0; i < list->length; i++)
        Release(list->objects[i]);

    list->length = 0;
}

void argon::vm::datatype::ListRemove(List *list, ArSSize index) {
    std::unique_lock _(list->rwlock);

    if (index < 0)
        index = ((ArSSize) list->length) + index;

    if (index >= list->length)
        return;

    Release(list->objects[index]);

    // Move items back
    for (ArSize i = index + 1; i < list->length; i++)
        list->objects[i - 1] = list->objects[i];

    list->length--;
}

// LIST ITERATOR

ArObject *listiterator_iter_next(ListIterator *self) {
    if (self->iterable == nullptr)
        return nullptr;

    if (!self->reverse) {
        if (self->index < self->iterable->length) {
            auto *tmp = IncRef(self->iterable->objects[self->index]);

            self->index++;

            return tmp;
        }
        return nullptr;
    }

    if (self->iterable->length - self->index == 0)
        return nullptr;

    self->index++;

    return IncRef(self->iterable->objects[self->iterable->length - self->index]);
}

bool listiterator_is_true(ListIterator *self) {
    std::unique_lock iter_lock(self->lock);
    std::shared_lock iterable_lock(self->iterable->rwlock);

    return (self->iterable->length - self->index) > 0;
}

TypeInfo ListIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "ListIterator",
        nullptr,
        nullptr,
        sizeof(ListIterator),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) IteratorDtor,
        (TraceOp) IteratorTrace,
        nullptr,
        (Bool_UnaryOp) listiterator_is_true,
        nullptr,
        nullptr,
        nullptr,
        IteratorIter,
        (UnaryOp) listiterator_iter_next,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_list_iterator_ = &ListIteratorType;