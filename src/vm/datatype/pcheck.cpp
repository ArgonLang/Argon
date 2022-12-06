// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "atom.h"
#include "arstring.h"
#include "boolean.h"
#include "bounds.h"
#include "bytes.h"
#include "code.h"
#include "decimal.h"
#include "dict.h"
#include "error.h"
#include "future.h"
#include "integer.h"
#include "module.h"
#include "nil.h"
#include "set.h"

#include "pcheck.h"

using namespace argon::vm::datatype;

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
        if (!InitParam(pc->params, &description)) {
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