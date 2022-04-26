// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cstdarg>

#include <vm/runtime.h>
#include <memory/memory.h>

#include "bool.h"
#include "bounds.h"
#include "decimal.h"
#include "error.h"
#include "list.h"
#include "nil.h"
#include "integer.h"
#include "iterator.h"
#include "string.h"
#include "tuple.h"

using namespace argon::object;

ArSize tuple_len(Tuple *self) {
    return self->len;
}

ArObject *argon::object::TupleGetItem(Tuple *self, ArSSize index) {
    ArObject *obj;

    if (index < 0)
        index = self->len + index;

    if (index < self->len) {
        obj = self->objects[index];
        IncRef(obj);
        return obj;
    }

    return ErrorFormat(type_overflow_error_, "tuple index out of range (len: %d, idx: %d)", self->len, index);
}

ArObject *tuple_get_slice(Tuple *self, Bounds *bounds) {
    ArObject *tmp;
    Tuple *ret;

    ArSSize slice_len;
    ArSSize start;
    ArSSize stop;
    ArSSize step;

    slice_len = BoundsIndex(bounds, self->len, &start, &stop, &step);

    if ((ret = TupleNew(slice_len)) == nullptr)
        return nullptr;

    if (step >= 0) {
        for (ArSize i = 0; start < stop; start += step) {
            tmp = self->objects[start];
            IncRef(tmp);
            ret->objects[i++] = tmp;
        }
    } else {
        for (ArSize i = 0; stop < start; start += step) {
            tmp = self->objects[start];
            IncRef(tmp);
            ret->objects[i++] = tmp;
        }
    }

    return ret;
}

const SequenceSlots tuple_sequence{
        (SizeTUnaryOp) tuple_len,
        (BinaryOpArSize) argon::object::TupleGetItem,
        nullptr,
        (BinaryOp) tuple_get_slice,
        nullptr
};

ARGON_FUNCTION5(tuple_, new, "Creates an empty tuple or construct it from an iterable object."
                             ""
                             "- Parameter [iter]: iterable object."
                             "- Returns: new tuple.", 0, true) {
    if (!VariadicCheckPositional("tuple::new", count, 0, 1))
        return nullptr;

    if (count == 1)
        return TupleNew(*argv);

    return TupleNew((ArSize) 0);
}

ARGON_METHOD5(tuple_, find,
              "Find an item into the tuple and returns its position."
              ""
              "- Parameter obj: object to search."
              "- Returns: index if the object was found into the tuple, -1 otherwise.", 1, false) {
    auto *tuple = (Tuple *) self;

    for (ArSize i = 0; i < tuple->len; i++) {
        if (Equal(tuple->objects[i], *argv))
            return IntegerNew(i);
    }

    return IntegerNew(-1);
}

const NativeFunc tuple_methods[] = {
        tuple_new_,
        tuple_find_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots tuple_obj = {
        tuple_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

bool tuple_is_true(Tuple *self) {
    return self->len > 0;
}

ArObject *tuple_compare(Tuple *self, ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other) {
        auto t1 = (Tuple *) self;
        auto t2 = (Tuple *) other;

        if (t1->len != t2->len)
            return BoolToArBool(false);

        for (ArSize i = 0; i < t1->len; i++)
            if (!Equal(t1->objects[i], t2->objects[i]))
                return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ArSize tuple_hash(Tuple *self) {
    unsigned long result = 1;
    ArObject *obj;
    ArSize hash;

    if (self->len == 0)
        return 0;

    for (ArSize i = 0; i < self->len; i++) {
        obj = self->objects[i];

        if ((hash = Hash(obj)) == 0 && argon::vm::IsPanicking())
            return 0;

        result = 31 * result + hash;
    }

    return result;
}

ArObject *tuple_str(Tuple *self) {
    StringBuilder sb = {};
    String *tmp = nullptr;

    if (StringBuilderWrite(&sb, (unsigned char *) "(", 1, self->len == 0 ? 1 : 0) < 0)
        goto error;

    for (ArSize i = 0; i < self->len; i++) {
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

    if (StringBuilderWrite(&sb, (unsigned char *) ")", 1) < 0)
        goto error;

    return StringBuilderFinish(&sb);

    error:
    Release(tmp);
    StringBuilderClean(&sb);
    return nullptr;
}

ArObject *tuple_iter_get(List *self) {
    return IteratorNew(self, false);
}

ArObject *tuple_iter_rget(List *self) {
    return IteratorNew(self, true);
}

void tuple_cleanup(Tuple *self) {
    for (ArSize i = 0; i < self->len; i++)
        Release(self->objects[i]);

    argon::memory::Free(self->objects);
}

const TypeInfo TupleType = {
        TYPEINFO_STATIC_INIT,
        "tuple",
        nullptr,
        sizeof(Tuple),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) tuple_cleanup,
        nullptr,
        (CompareOp) tuple_compare,
        (BoolUnaryOp) tuple_is_true,
        (SizeTUnaryOp) tuple_hash,
        (UnaryOp) tuple_str,
        (UnaryOp) tuple_iter_get,
        (UnaryOp) tuple_iter_rget,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &tuple_obj,
        &tuple_sequence,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_tuple_ = &TupleType;

template<typename T>
Tuple *TupleClone(T *t) {
    Tuple *tuple;

    if ((tuple = TupleNew(t->len)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < t->len; i++) {
        IncRef(t->objects[i]);
        tuple->objects[i] = t->objects[i];
    }

    tuple->len = t->len;
    return tuple;
}

Tuple *argon::object::TupleNew(ArSize len) {
    auto tuple = ArObjectNew<Tuple>(RCType::INLINE, type_tuple_);

    if (tuple != nullptr) {
        tuple->objects = nullptr;
        tuple->len = len;

        if (len > 0) {
            if ((tuple->objects = (ArObject **) argon::memory::Alloc(len * sizeof(void *))) == nullptr) {
                Release(tuple);
                return (Tuple *) argon::vm::Panic(error_out_of_memory);
            }

            for (ArSize i = 0; i < len; i++) {
                IncRef(NilVal);
                tuple->objects[i] = NilVal;
            }
        }
    }

    return tuple;
}

Tuple *argon::object::TupleNew(const ArObject *sequence) {
    List *tmp;
    Tuple *tuple;

    if (AsSequence(sequence)) {
        // FAST PATH
        if (AR_TYPEOF(sequence, type_list_)) {
            RWLockRead list_lock(((List *) sequence)->lock);
            return TupleClone((List *) sequence);
        } else if (AR_TYPEOF(sequence, type_tuple_))
            return TupleClone((Tuple *) sequence);
    }

    if (IsIterable(sequence)) {
        if ((tmp = ListNew(sequence)) == nullptr)
            return nullptr;

        tuple = TupleClone(tmp);
        Release(tmp);

        if (tuple == nullptr)
            return nullptr;

        return tuple;
    }

    return (Tuple *) ErrorFormat(type_not_implemented_, "no viable conversion from '%s' to tuple",
                                 AR_TYPE_NAME(sequence));
}

Tuple *argon::object::TupleNew(ArObject *result, ArObject *error) {
    Tuple *tuple;

    if ((tuple = TupleNew(2)) != nullptr) {
        if (result == nullptr)
            result = IncRef(NilVal);

        if (error == nullptr)
            error = IncRef(NilVal);

        TupleInsertAt(tuple, 0, result);
        TupleInsertAt(tuple, 1, error);
        return tuple;
    }

    return nullptr;
}

Tuple *argon::object::TupleNew(ArObject **objects, ArSize count) {
    auto *tuple = TupleNew(count);

    if (tuple != nullptr) {
        for (ArSize i = 0; i < count; i++)
            tuple->objects[i] = IncRef(objects[i]);
    }

    return tuple;
}

Tuple *argon::object::TupleNew(const char *fmt, ...) {
    va_list args;
    ArObject *obj;
    Tuple *tuple;
    const void *tmp;

    ArSize flen = strlen(fmt);

    if ((tuple = TupleNew(flen)) == nullptr)
        return nullptr;

    va_start(args, fmt);

    for (int i = 0; i < flen; i++) {
        switch (fmt[i]) {
            case 'a':
            case 'A':
                obj = IncRef(va_arg(args, ArObject*));
                if (obj == nullptr)
                    obj = ARGON_OBJECT_NIL;
                break;
            case 's':
            case 'S':
                tmp = va_arg(args, const char*);
                obj = tmp == nullptr ? StringIntern("") : StringNew((const char *) tmp);
                break;
            case 'd':
            case 'D':
            case 'f':
            case 'F':
                obj = DecimalNew(va_arg(args, DecimalUnderlying));
                break;
            case 'i':
                obj = IntegerNew((int) va_arg(args, ArSSize));
                break;
            case 'I':
                obj = IntegerNew((unsigned int) va_arg(args, ArSSize));
                break;
            case 'l':
                obj = IntegerNew((long) va_arg(args, ArSSize));
                break;
            case 'h':
                obj = IntegerNew((short) va_arg(args, ArSSize));
                break;
            case 'H':
                obj = IntegerNew((unsigned short) va_arg(args, ArSSize));
                break;
            default:
                ErrorFormat(type_value_error_, "TupleNew: unexpected '%c' in fmt string", fmt[i]);
                Release(tuple);
                return nullptr;
        }

        if (obj == nullptr) {
            Release(tuple);
            return nullptr;
        }

        TupleInsertAt(tuple, i, obj);
        Release(obj);
    }

    return tuple;
}

bool argon::object::TupleInsertAt(Tuple *tuple, ArSize idx, ArObject *obj) {
    if (idx >= tuple->len)
        return false;

    Release(tuple->objects[idx]);

    if (obj == nullptr)
        obj = NilVal;

    tuple->objects[idx] = IncRef(obj);

    return true;
}

bool argon::object::TupleUnpack(Tuple *tuple, const char *fmt, ...) {
    va_list args;
    ArObject *obj;
    ArSize flen;

    va_start(args, fmt);

    flen = strlen(fmt);

    if (tuple->len < flen) {
        va_end(args);
        return ErrorFormat(type_value_error_, "TupleUnpack: length of the tuple does not match the length of fmt");
    }

    for (int i = 0; i < flen; i++) {
        obj = tuple->objects[i];
        switch (fmt[i]) {
            case 'a':
            case 'A':
                *va_arg(args, ArObject**) = IncRef(tuple->objects[i]);
                break;
            case 's':
            case 'S':
                if (!AR_TYPEOF(obj, type_string_)) {
                    va_end(args);
                    return ErrorFormat(type_type_error_, "TupleUnpack: expected '%s' in index %d, not '%s'",
                                       type_string_->name, i, AR_TYPE_NAME(obj));
                }

                *va_arg(args, const char **) = (const char *) ((String *) obj)->buffer;
                break;
            case 'd':
            case 'D':
            case 'f':
            case 'F':
                if (!AR_TYPEOF(obj, type_decimal_)) {
                    va_end(args);
                    return ErrorFormat(type_type_error_, "TupleUnpack: expected '%s' in index %d, not '%s'",
                                       type_decimal_->name, AR_TYPE_NAME(obj));
                }

                *va_arg(args, double *) = (double) ((Decimal *) obj)->decimal;
                break;
            case 'i':
            case 'I':
                if (!AR_TYPEOF(obj, type_integer_)) {
                    va_end(args);
                    return ErrorFormat(type_type_error_, "TupleUnpack: expected '%s' in index %d, not '%s'",
                                       type_integer_->name, AR_TYPE_NAME(obj));
                }

                *va_arg(args, int *) = (int) ((Integer *) obj)->integer;
                break;
            case 'l':
                if (!AR_TYPEOF(obj, type_integer_)) {
                    va_end(args);
                    return ErrorFormat(type_type_error_, "TupleUnpack: expected '%s' in index %d, not '%s'",
                                       type_integer_->name, AR_TYPE_NAME(obj));
                }

                *va_arg(args, long *) = (long) ((Integer *) obj)->integer;
                break;
            case 'h':
            case 'H':
                if (!AR_TYPEOF(obj, type_integer_)) {
                    va_end(args);
                    return ErrorFormat(type_type_error_, "TupleUnpack: expected '%s' in index %d, not '%s'",
                                       type_integer_->name, AR_TYPE_NAME(obj));
                }

                *va_arg(args, short *) = (short) ((Integer *) obj)->integer;
                break;
            default:
                ErrorFormat(type_value_error_, "TupleUnpack: unexpected '%c' in  fmt string", fmt[i]);
                return false;
        }
    }

    va_end(args);
    return true;
}