// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"
#include "boolean.h"
#include "error.h"
#include "function.h"
#include "pcheck.h"

#include "option.h"

using namespace argon::vm::datatype;

ARGON_FUNCTION(option_option, Option,
               "Returns new option that can encapsulate an optional object.\n"
               "\n"
               "- Parameter obj: Optional object.\n"
               "- Returns: Option<?>.\n",
               nullptr, true, false) {
    if (!VariadicCheckPositional(option_option.name, argc, 0, 1))
        return nullptr;

    if (argc == 1)
        return (ArObject *) OptionNew(*args);

    return (ArObject *) OptionNew();
}

ARGON_METHOD(option_unwrap, unwrap,
             "Returns contained value.\n"
             "\n"
             "If contained value is empty this method panic.\n"
             "\n"
             "- Returns: Contained value.\n",
             nullptr, false, false) {
    auto *self = (Option *) _self;

    if (self->some == nullptr) {
        ErrorFormat(kValueError[0], "%s on a empty value", ARGON_RAW_STRING(((Function *) _func)->qname));
        return nullptr;
    }

    return IncRef(self->some);
}

ARGON_METHOD(option_unwrap_or, unwrap_or,
             "Returns the contained value or a provided default.\n"
             "\n"
             "Default value are eagerly evaluated.\n"
             "\n"
             "- Parameters value: Default value.\n"
             "- Returns: Contained object or default value.\n",
             ": value", false, false) {
    auto *self = (Option *) _self;

    if (self->some == nullptr)
        return IncRef(*args);

    return IncRef(self->some);
}

const FunctionDef option_methods[] = {
        option_option,

        option_unwrap,
        option_unwrap_or,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots option_objslot = {
        option_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *option_compare(const Option *self, const ArObject *other, CompareMode mode) {
    const auto *o = (const Option *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    if (self->some == o->some)
        return AR_GET_TYPE(self)->compare(self->some, o->some, mode);

    return BoolToArBool(false);
}

ArObject *option_repr(const Option *self) {
    return (ArObject *) StringFormat("Option<%s>", self->some == nullptr ? "?" : AR_TYPE_NAME(self->some));
}

bool option_dtor(Option *self) {
    Release(self->some);

    return true;
}

bool option_is_true(const Option *self) {
    return self->some != nullptr;
}

TypeInfo OptionType = {
        AROBJ_HEAD_INIT_TYPE,
        "Option",
        nullptr,
        nullptr,
        sizeof(Option),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) option_dtor,
        nullptr,
        nullptr,
        (Bool_UnaryOp) option_is_true,
        (CompareOp) option_compare,
        (UnaryConstOp) option_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &option_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_option_ = &OptionType;

Option *argon::vm::datatype::OptionNew(ArObject *value) {
    auto *opt = MakeGCObject<Option>(&OptionType, false);

    if (opt != nullptr)
        opt->some = IncRef(value);

    return opt;
}