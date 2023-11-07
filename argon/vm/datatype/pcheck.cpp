// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cctype>

#include <argon/vm/datatype/atom.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bounds.h>
#include <argon/vm/datatype/bytes.h>
#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/decimal.h>
#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/function.h>
#include <argon/vm/datatype/future.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/module.h>
#include <argon/vm/datatype/nil.h>
#include <argon/vm/datatype/set.h>

#include <argon/vm/datatype/pcheck.h>

using namespace argon::vm::datatype;

ArObject *pcheck_compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    if (mode == CompareMode::EQ)
        return BoolToArBool(self == other);

    return nullptr;
}

bool pcheck_dtor(PCheck *self) {
    for (int i = 0; i < self->count; i++) {
        auto *param = self->params[i];

        argon::vm::memory::Free(param->name);
        argon::vm::memory::Free(param);
    }

    argon::vm::memory::Free(self->params);

    return true;
}

TypeInfo PCheckType = {
        AROBJ_HEAD_INIT_TYPE,
        "PCheck",
        nullptr,
        nullptr,
        sizeof(PCheck),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) pcheck_dtor,
        nullptr,
        nullptr,
        nullptr,
        pcheck_compare,
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
const TypeInfo *argon::vm::datatype::type_pcheck_ = &PCheckType;

unsigned short CountParams(const char *format) {
    if (format == nullptr || *format == '\0')
        return 0;

    unsigned short count = 1;

    while (*format != 0) {
        if (*format++ == ',')
            count++;
    }

    return count;
}

unsigned short CountTypes(const char *format, const char **start_name) {
    unsigned short count = 0;

    while (*format != ':' && *format != 0) {
        if ((*format >= 'a' && *format <= 'z') || (*format >= 'A' && *format <= 'Z') || *format == '?')
            count++;

        format++;
    }

    *start_name = format;

    return count;
}

void SetType(Param *param, const char *start, const char *end) {
    int index = 0;

    while (start < end) {
        switch (*start++) {
            case 'a':
                param->types[index++] = type_atom_;
                break;
            case 'b':
                param->types[index++] = type_boolean_;
                break;
            case 'B':
                param->types[index++] = type_bounds_;
                break;
            case 'c':
                param->types[index++] = type_code_;
                break;
            case 'd':
                param->types[index++] = type_decimal_;
                break;
            case 'D':
                param->types[index++] = type_dict_;
                break;
            case 'e' :
                param->types[index++] = type_error_;
                break;
            case 'f':
                param->types[index++] = type_future_;
                break;
            case 'F':
                param->types[index++] = type_function_;
                break;
            case 'h':
                assert(false);
            case 'i':
                param->types[index++] = type_int_;
                break;
            case 'l':
                param->types[index++] = type_list_;
                break;
            case 'm':
                param->types[index++] = type_module_;
                break;
            case 'n':
                param->types[index++] = type_nil_;
                break;
            case 'N':
                param->types[index++] = type_namespace_;
                break;
            case 'r' :
                param->types[index++] = type_result_;
                break;
            case 's':
                param->types[index++] = type_string_;
                break;
            case 'S':
                param->types[index++] = type_set_;
                break;
            case 't':
                param->types[index++] = type_tuple_;
                break;
            case 'u' :
                param->types[index++] = type_uint_;
                break;
            case 'x':
                param->types[index++] = type_bytes_;
                break;
            default:
                break; // Ignore!
        }
    }
}

bool InitParam(Param **param, const char **descriptor) {
    const char *type_start = *descriptor;
    const char *type_end;
    const char *start_name;
    char *name;

    ArSize name_length;
    ArSize type_length;

    type_length = CountTypes(*descriptor, &start_name) + 1;

    type_end = start_name;

    if (*start_name != ':') {
        ErrorFormat(kValueError[0], "expected ':' after type[s] definition");
        return false;
    }

    start_name++; // Ignore :

    while (*start_name == ' ')
        start_name++;

    if (!std::isalpha(*start_name)) {
        ErrorFormat(kValueError[0], "expected valid parameter name here: %s", start_name);
        return false;
    }

    name_length = 0;
    while (std::isalnum(*(start_name + name_length)))
        name_length++;

    *descriptor = start_name + name_length;

    if ((name = (char *) argon::vm::memory::Alloc(name_length + 1)) == nullptr)
        return false;

    argon::vm::memory::MemoryCopy(name, start_name, name_length);
    name[name_length] = '\0';

    // Build argument
    *param = (Param *) argon::vm::memory::Alloc(sizeof(Param) + (type_length * sizeof(void *)));
    if (*param == nullptr) {
        argon::vm::memory::Free(name);
        return false;
    }

    (*param)->name = name;

    if (type_start != type_end)
        SetType(*param, type_start, type_end);

    (*param)->types[type_length - 1] = nullptr; // sentinel

    return true;
}

PCheck *argon::vm::datatype::PCheckNew(const char *description) {
    auto *pc = MakeObject<PCheck>(type_pcheck_);
    if (pc == nullptr)
        return nullptr;

    pc->count = CountParams(description);

    if (pc->count == 0) {
        pc->params = nullptr;
        return pc;
    }

    if ((pc->params = (Param **) argon::vm::memory::Alloc(pc->count * sizeof(void *))) == nullptr) {
        Release(pc);
        return nullptr;
    }

    unsigned short index = 0;
    while (index < pc->count && *description != '\0') {
        if (!InitParam(pc->params + index, &description)) {
            Release(pc);
            return nullptr;
        }

        if (*description == ',')
            description++;

        index++;
    }

    if (index != pc->count) {
        Release(pc);

        ErrorFormat(kValueError[0], "params definition string is malformed from this point: %s", description);

        return nullptr;
    }

    return pc;
}

bool argon::vm::datatype::VariadicCheckPositional(const char *name, unsigned int nargs, unsigned int min,
                                                  unsigned int max) {
    if (nargs < min) {
        ErrorFormat(kTypeError[0], "%s expected %s%d argument%s, got %d", name,
                    (min == max ? "" : "at least "), min, min == 1 ? "" : "s", nargs);

        return false;
    } else if (max > min && nargs > max) {
        ErrorFormat(kTypeError[0], "%s expected %s%d argument%s, got %d", name,
                    (min == max ? "" : "at most "), max, max == 1 ? "" : "s", nargs);

        return false;
    }

    return true;
}

// KWParameters utilities

bool argon::vm::datatype::KParamLookupBool(Dict *kwargs, const char *key, bool *out, bool _default) {
    ArObject *obj;

    if (kwargs == nullptr) {
        if (out != nullptr)
            *out = _default;

        return true;
    }

    if (!DictLookup(kwargs, key, &obj))
        return false;

    if (obj == nullptr) {
        if (out != nullptr)
            *out = _default;

        return true;
    }

    if (out != nullptr)
        *out = IsTrue(obj);

    Release(obj);

    return true;
}

bool argon::vm::datatype::KParamLookupInt(Dict *kwargs, const char *key,
                                          IntegerUnderlying *out,
                                          IntegerUnderlying _default) {
    ArObject *obj;

    if (kwargs == nullptr) {
        if (out != nullptr)
            *out = _default;

        return true;
    }

    if (!DictLookup(kwargs, key, &obj))
        return false;

    if (obj == nullptr) {
        if (out != nullptr)
            *out = _default;

        return true;
    }

    if (!AR_TYPEOF(obj, type_int_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], type_int_->name, AR_TYPE_QNAME(obj));

        Release(obj);
        return false;
    }

    if (out != nullptr)
        *out = ((Integer *) obj)->sint;

    Release(obj);

    return true;
}

bool argon::vm::datatype::KParamLookupStr(Dict *kwargs, const char *key, String **out,
                                          const char *_default, bool *out_isdef) {
    ArObject *obj = nullptr;

    if (kwargs != nullptr && !DictLookup(kwargs, key, &obj))
        return false;

    if (obj == nullptr) {
        if (out_isdef != nullptr) {
            *out_isdef = true;

            return true;
        }

        if(_default == nullptr)
            return true;

        *out = StringNew(_default);

        return *out != nullptr;
    }

    if (out_isdef != nullptr)
        *out_isdef = false;

    if (!AR_TYPEOF(obj, type_string_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], type_string_->name, AR_TYPE_QNAME(obj));

        Release(obj);
        return false;
    }

    *out = (String *) obj;

    return true;
}

bool argon::vm::datatype::KParamLookupUInt(Dict *kwargs, const char *key,
                                           UIntegerUnderlying *out,
                                           UIntegerUnderlying _default) {
    ArObject *obj;

    if (kwargs == nullptr) {
        if (out != nullptr)
            *out = _default;

        return true;
    }

    if (!DictLookup(kwargs, key, &obj))
        return false;

    if (obj == nullptr) {
        if (out != nullptr)
            *out = _default;

        return true;
    }

    if (!AR_TYPEOF(obj, type_uint_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], type_uint_->name, AR_TYPE_QNAME(obj));

        Release(obj);
        return false;
    }

    if (out != nullptr)
        *out = ((Integer *) obj)->uint;

    Release(obj);

    return true;
}

