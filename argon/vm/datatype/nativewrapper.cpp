// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/decimal.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/nil.h>
#include <argon/vm/datatype/nativewrapper.h>

using namespace argon::vm::datatype;

constexpr const char *NativeTypeStr[] = {"BOOL", "DOUBLE", "FLOAT", "INT", "LONG", "OBJECT",
                                         "SHORT", "STRING", "UINT", "ULONG", "USHORT"};

TypeInfo NativeWrapperType = {
        AROBJ_HEAD_INIT_TYPE,
        "NativeWrapper",
        nullptr,
        nullptr,
        sizeof(NativeWrapper),
        TypeInfoFlags::BASE,
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
const TypeInfo *argon::vm::datatype::type_native_wrapper_ = &NativeWrapperType;

NativeWrapper *argon::vm::datatype::NativeWrapperNew(const MemberDef *member) {
    auto *nw = MakeObject<NativeWrapper>(type_native_wrapper_);

    if (nw != nullptr)
        argon::vm::memory::MemoryCopy(&nw->member, member, sizeof(MemberDef));

    return nw;
}

#define GET_MEMBER(wrapper, native) (((unsigned char*) (native)) + (wrapper)->member.offset)

bool SetNumber(const NativeWrapper *wrapper, const ArObject *native, const ArObject *value) {
    if (AR_TYPEOF(value, type_int_) || AR_TYPEOF(value, type_uint_)) {
        const auto *num = (const Integer *) value;

        switch (wrapper->member.type) {
            case MemberType::INT:
                *((int *) GET_MEMBER(wrapper, native)) = num->sint;
                return true;
            case MemberType::LONG:
                *((long *) GET_MEMBER(wrapper, native)) = num->sint;
                return true;
            case MemberType::SHORT:
                *((short *) GET_MEMBER(wrapper, native)) = (short) num->sint;
                return true;
            case MemberType::UINT:
                *((unsigned int *) GET_MEMBER(wrapper, native)) = num->uint;
                return true;
            case MemberType::ULONG:
                *((unsigned long *) GET_MEMBER(wrapper, native)) = num->uint;
                return true;
            case MemberType::USHORT:
                *((unsigned short *) GET_MEMBER(wrapper, native)) = (unsigned short) num->uint;
                return true;
            case MemberType::DOUBLE:
                *((double *) GET_MEMBER(wrapper, native)) = num->sint;
                return true;
            case MemberType::FLOAT:
                *((float *) GET_MEMBER(wrapper, native)) = (float) num->sint;
                return true;
            default:
                break;
        }
    }

    if (AR_TYPEOF(value, type_decimal_)) {
        const auto *num = (const Decimal *) value;

        switch (wrapper->member.type) {
            case MemberType::DOUBLE:
                *((double *) GET_MEMBER(wrapper, native)) = (double) num->decimal;
                return true;
            case MemberType::FLOAT:
                *((float *) GET_MEMBER(wrapper, native)) = (float) num->decimal;
                return true;
            default:
                break;
        }
    }

    ErrorFormat(kTypeError[0], "no viable conversion from '%s' to %s::%s(%s)", AR_TYPE_NAME(value),
                AR_TYPE_NAME(native), wrapper->member.name, NativeTypeStr[(int) wrapper->member.type]);

    return false;
}

bool SetString(char **string, const ArObject *value) {
    auto *str = (String *) Str((ArObject *) value);
    char *tmp;

    if (str == nullptr)
        return false;

    tmp = (char *) argon::vm::memory::Alloc(ARGON_RAW_STRING_LENGTH(str) + 1);
    if (tmp == nullptr) {
        Release(str);
        return false;
    }

    argon::vm::memory::MemoryCopy(tmp, ARGON_RAW_STRING(str), ARGON_RAW_STRING_LENGTH(str));
    tmp[ARGON_RAW_STRING_LENGTH(str)] = '\0';

    Release(str);
    argon::vm::memory::Free((void *) *string);

    *string = tmp;

    return true;
}

bool argon::vm::datatype::NativeWrapperSet(const NativeWrapper *wrapper, ArObject *native, ArObject *value) {
    if (wrapper->member.readonly || wrapper->member.offset < 0 && wrapper->member.set == nullptr) {
        ErrorFormat(kUnassignableError[0], kUnassignableError[2], AR_TYPE_NAME(native), wrapper->member.name);
        return false;
    }

    if (wrapper->member.set != nullptr)
        return wrapper->member.set(native, value);
    else if (wrapper->member.get != nullptr) {
        ErrorFormat(kUnassignableError[0], kUnassignableError[2], AR_TYPE_NAME(native), wrapper->member.name);
        return false;
    }

    switch (wrapper->member.type) {
        case MemberType::BOOL:
            *((bool *) GET_MEMBER(wrapper, native)) = IsTrue(value);
            break;
        case MemberType::DOUBLE:
        case MemberType::FLOAT:
        case MemberType::INT:
        case MemberType::LONG:
        case MemberType::SHORT:
        case MemberType::UINT:
        case MemberType::ULONG:
        case MemberType::USHORT:
            return SetNumber(wrapper, native, value);
        case MemberType::OBJECT: {
            auto **obj = (ArObject **) GET_MEMBER(wrapper, native);

            Release(*obj);

            *obj = IncRef(value);

            break;
        }
        case MemberType::STRING:
            if (!SetString((char **) GET_MEMBER(wrapper, native), value))
                return false;
            break;
        default:
            ErrorFormat(kRuntimeError[0], kRuntimeError[4], AR_TYPE_NAME(native), wrapper->member.name);
            return false;
    }

    return true;
}

ArObject *argon::vm::datatype::NativeWrapperGet(const NativeWrapper *wrapper, const ArObject *native) {
    void *tmp;

    if (wrapper->member.get != nullptr)
        return wrapper->member.get(native);
    else if (wrapper->member.set != nullptr) {
        ErrorFormat(kUnassignableError[0], kUnassignableError[3], AR_TYPE_NAME(native), wrapper->member.name);
        return nullptr;
    }

    switch (wrapper->member.type) {
        case MemberType::BOOL:
            return BoolToArBool(*((bool *) GET_MEMBER(wrapper, native)));
        case MemberType::DOUBLE:
            return (ArObject *) DecimalNew(*((double *) GET_MEMBER(wrapper, native)));
        case MemberType::FLOAT:
            return (ArObject *) DecimalNew(*((float *) GET_MEMBER(wrapper, native)));
        case MemberType::INT:
            return (ArObject *) IntNew(*((int *) GET_MEMBER(wrapper, native)));
        case MemberType::LONG:
            return (ArObject *) IntNew(*((long *) GET_MEMBER(wrapper, native)));
        case MemberType::OBJECT:
            tmp = *((ArObject **) GET_MEMBER(wrapper, native));
            if (tmp == nullptr)
                return (ArObject *) IncRef(Nil);

            return IncRef((ArObject *) tmp);
        case MemberType::SHORT:
            return (ArObject *) UIntNew(*((short *) GET_MEMBER(wrapper, native)));
        case MemberType::STRING:
            tmp = *((char **) GET_MEMBER(wrapper, native));
            if (tmp == nullptr)
                return (ArObject *) IncRef(Nil);

            return (ArObject *) StringNew((const char *) tmp);
        case MemberType::UINT:
            return (ArObject *) UIntNew(*((unsigned int *) GET_MEMBER(wrapper, native)));
        case MemberType::ULONG:
            return (ArObject *) UIntNew(*((unsigned long *) GET_MEMBER(wrapper, native)));
        case MemberType::USHORT:
            return (ArObject *) UIntNew(*((unsigned short *) GET_MEMBER(wrapper, native)));
        default:
            ErrorFormat(kRuntimeError[0], "NativeWrapperGet: invalid MemberType");
            return nullptr;
    }
}