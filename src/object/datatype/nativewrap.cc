// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <vm/runtime.h>

#include "decimal.h"
#include "error.h"
#include "integer.h"
#include "nil.h"
#include "string.h"

#include "nativewrap.h"
#include "bool.h"

using namespace argon::object;

const char *NativeTypeStr[] = {"arobject", "bool", "double", "float", "int", "long", "short", "string"};

const TypeInfo NativeWrapperType = {
        TYPEINFO_STATIC_INIT,
        "NativeWrapper",
        nullptr,
        sizeof(NativeWrapper),
        TypeInfoFlags::STRUCT,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_native_wrapper_ = &NativeWrapperType;

NativeWrapper *argon::object::NativeWrapperNew(const NativeMember *member) {
    auto *native = ArObjectNew<NativeWrapper>(RCType::INLINE, type_native_wrapper_);
    ArSize lname = strlen(member->name);

    if (native != nullptr) {
        if ((native->name = (char *) memory::Alloc(lname)) == nullptr) {
            Release(native);
            return nullptr;
        }

        memory::MemoryCopy(native->name, member->name, lname);
        native->mtype = member->type;
        native->offset = member->offset;
        native->readonly = member->readonly;
    }

    return native;
}

#define offset (((unsigned char*)native) + wrapper->offset)

ArObject *argon::object::NativeWrapperGet(const NativeWrapper *wrapper, const ArObject *native) {
    ArObject *obj = nullptr;
    void *tmp;

    switch (wrapper->mtype) {
        case NativeMemberType::AROBJECT:
            if ((tmp = *((ArObject **) offset)) != nullptr)
                obj = IncRef((ArObject *) tmp);
            else
                obj = IncRef(NilVal);
            break;
        case NativeMemberType::BOOL:
            obj = BoolToArBool(*((bool *) offset));
            break;
        case NativeMemberType::DOUBLE:
            obj = DecimalNew(*((double *) offset));
            break;
        case NativeMemberType::FLOAT:
            obj = DecimalNew(*((float *) offset));
            break;
        case NativeMemberType::INT:
            obj = IntegerNew(*((int *) offset));
            break;
        case NativeMemberType::LONG:
            obj = IntegerNew(*((long *) offset));
            break;
        case NativeMemberType::SHORT:
            obj = IntegerNew(*((short *) offset));
            break;
        case NativeMemberType::STRING:
            if ((tmp = *((char **) offset)) != nullptr)
                obj = StringNew((char *) tmp);
            else
                obj = IncRef(NilVal);
            break;
    }

    return obj;
}

bool ExtractNumberOrError(const NativeWrapper *wrapper, const ArObject *native, const ArObject *value,
                          IntegerUnderlying *integer, DecimalUnderlying *decimal) {
    if (AR_TYPEOF(value, type_integer_)) {
        if (integer != nullptr)
            *integer = ((Integer *) value)->integer;
        else
            *decimal = (DecimalUnderlying) ((Integer *) value)->integer;
        return true;
    }

    if (AR_TYPEOF(value, type_decimal_)) {
        if (integer != nullptr)
            *integer = (IntegerUnderlying) ((Decimal *) value)->decimal;
        else
            *decimal = ((Decimal *) value)->decimal;
        return true;
    }

    ErrorFormat(type_type_error_, "no viable conversion from '%s' to %s::%s(%s)", AR_TYPE_NAME(value),
                AR_TYPE_NAME(native), wrapper->name, NativeTypeStr[(int) wrapper->mtype]);
    return false;
}

bool argon::object::NativeWrapperSet(const NativeWrapper *wrapper, const ArObject *native, ArObject *value) {
    IntegerUnderlying integer;
    DecimalUnderlying decimal;

    if (wrapper->readonly) {
        ErrorFormat(type_unassignable_error_, "%s::%s is read-only", AR_TYPE_NAME(native), wrapper->name);
        return false;
    }

    switch (wrapper->mtype) {
        case NativeMemberType::AROBJECT: {
            auto **dst = (ArObject **) offset;
            Release(*dst);
            *dst = IncRef(value);
            break;
        }
        case NativeMemberType::BOOL:
            *((bool *) offset) = IsTrue(value);
            break;
        case NativeMemberType::DOUBLE:
            if (!ExtractNumberOrError(wrapper, native, value, nullptr, &decimal))
                return false;
            *((double *) offset) = (double) decimal;
            break;
        case NativeMemberType::FLOAT:
            if (!ExtractNumberOrError(wrapper, native, value, nullptr, &decimal))
                return false;
            *((float *) offset) = (float) decimal;
            break;
        case NativeMemberType::INT:
            if (!ExtractNumberOrError(wrapper, native, value, &integer, nullptr))
                return false;
            *((int *) offset) = (int) integer;
            break;
        case NativeMemberType::LONG:
            if (!ExtractNumberOrError(wrapper, native, value, &integer, nullptr))
                return false;
            *((long *) offset) = (long) integer;
            break;
        case NativeMemberType::SHORT:
            if (!ExtractNumberOrError(wrapper, native, value, &integer, nullptr))
                return false;
            *((short *) offset) = (short) integer;
            break;
        case NativeMemberType::STRING: {
            auto **str = (unsigned char **) offset;
            auto *repr = (String *) ToString(value);
            unsigned char *tmp;

            if (repr == nullptr)
                return false;

            if ((tmp = (unsigned char *) memory::Alloc(repr->len)) == nullptr) {
                Release(repr);
                argon::vm::Panic(error_out_of_memory);
                return false;
            }

            memory::MemoryCopy(tmp, repr->buffer, repr->len);

            Release(repr);
            memory::Free(*str);
            *str = tmp;
            break;
        }
    }

    return true;
}

#undef offset