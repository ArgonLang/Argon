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
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_native_wrapper_ = &NativeWrapperType;

NativeWrapper *argon::object::NativeWrapperNew(const NativeMember *member) {
    auto *native = ArObjectNew<NativeWrapper>(RCType::INLINE, type_native_wrapper_);
    ArSize lname = strlen(member->name);

    if (native != nullptr) {
        if ((native->name = (char *) memory::Alloc(lname + 1)) == nullptr) {
            Release(native);
            return nullptr;
        }

        memory::MemoryCopy(native->name, member->name, lname);
        native->name[lname] = '\0';
        native->get = member->get;
        native->set = member->set;
        native->offset = member->offset;
        native->mtype = member->type;
        native->readonly = member->readonly;
    }

    return native;
}

#define get_offset (((unsigned char*) native) + wrapper->offset)

ArObject *argon::object::NativeWrapperGet(const NativeWrapper *wrapper, const ArObject *native) {
    void *tmp;

    if (wrapper->get != nullptr)
        return wrapper->get(native);

    switch (wrapper->mtype) {
        case NativeMemberType::AROBJECT:
            if ((tmp = *((ArObject **) get_offset)) != nullptr)
                return IncRef((ArObject *) tmp);
            else
                return IncRef(NilVal);
        case NativeMemberType::BOOL:
            return BoolToArBool(*((bool *) get_offset));
        case NativeMemberType::DOUBLE:
            return DecimalNew(*((double *) get_offset));
        case NativeMemberType::FLOAT:
            return DecimalNew(*((float *) get_offset));
        case NativeMemberType::INT:
            return IntegerNew(*((int *) get_offset));
        case NativeMemberType::LONG:
            return IntegerNew(*((long *) get_offset));
        case NativeMemberType::SHORT:
            return IntegerNew(*((short *) get_offset));
        case NativeMemberType::STRING:
            if ((tmp = *((char **) get_offset)) != nullptr)
                return StringNew((char *) tmp);
            else
                return IncRef(NilVal);
        default:
            return nullptr;
    }
}

bool ExtractNumberOrError(const NativeWrapper *wrapper, const ArObject *native, const ArObject *value,
                          IntegerUnderlying *integer, DecimalUnderlying *decimal) {
    if (AR_TYPEOF(value, type_integer_)) {
        if (integer != nullptr)
            *integer = ((const Integer *) value)->integer;
        else
            *decimal = (DecimalUnderlying) ((const Integer *) value)->integer;
        return true;
    }

    if (AR_TYPEOF(value, type_decimal_)) {
        if (integer != nullptr)
            *integer = (IntegerUnderlying) ((const Decimal *) value)->decimal;
        else
            *decimal = ((const Decimal *) value)->decimal;
        return true;
    }

    ErrorFormat(type_type_error_, "no viable conversion from '%s' to %s::%s(%s)", AR_TYPE_NAME(value),
                AR_TYPE_NAME(native), wrapper->name, NativeTypeStr[(int) wrapper->mtype]);
    return false;
}

bool argon::object::NativeWrapperSet(const NativeWrapper *wrapper, ArObject *native, ArObject *value) {
    IntegerUnderlying integer;
    DecimalUnderlying decimal;

    if (wrapper->readonly || wrapper->offset < 0 && wrapper->set == nullptr) {
        ErrorFormat(type_unassignable_error_, "%s::%s is read-only", AR_TYPE_NAME(native), wrapper->name);
        return false;
    }

    if (wrapper->set != nullptr)
        return wrapper->set(native, value);

    switch (wrapper->mtype) {
        case NativeMemberType::AROBJECT: {
            auto **dst = (ArObject **) get_offset;
            Release(*dst);
            *dst = IncRef(value);
            break;
        }
        case NativeMemberType::BOOL:
            *((bool *) get_offset) = IsTrue(value);
            break;
        case NativeMemberType::DOUBLE:
            if (!ExtractNumberOrError(wrapper, native, value, nullptr, &decimal))
                return false;
            *((double *) get_offset) = (double) decimal;
            break;
        case NativeMemberType::FLOAT:
            if (!ExtractNumberOrError(wrapper, native, value, nullptr, &decimal))
                return false;
            *((float *) get_offset) = (float) decimal;
            break;
        case NativeMemberType::INT:
            if (!ExtractNumberOrError(wrapper, native, value, &integer, nullptr))
                return false;
            *((int *) get_offset) = (int) integer;
            break;
        case NativeMemberType::LONG:
            if (!ExtractNumberOrError(wrapper, native, value, &integer, nullptr))
                return false;
            *((long *) get_offset) = integer;
            break;
        case NativeMemberType::SHORT:
            if (!ExtractNumberOrError(wrapper, native, value, &integer, nullptr))
                return false;
            *((short *) get_offset) = (short) integer;
            break;
        case NativeMemberType::STRING: {
            auto **str = (unsigned char **) get_offset;
            auto *repr = (String *) ToString(value);
            unsigned char *tmp;

            if (repr == nullptr)
                return false;

            if ((tmp = ArObjectNewRaw<unsigned char *>(repr->len + 1)) == nullptr) {
                Release(repr);
                return false;
            }

            memory::MemoryCopy(tmp, repr->buffer, repr->len);
            tmp[repr->len] = '\0';

            Release(repr);
            memory::Free(*str);
            *str = tmp;
            break;
        }
        default:
            ErrorFormat(type_runtime_error_, "unknown native type for the %s::%s property",
                        AR_TYPE_NAME(native), wrapper->name);
            return false;
    }

    return true;
}

#undef get_offset