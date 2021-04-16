// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bool.h"
#include "error.h"
#include "string.h"

#include "option.h"

using namespace argon::object;

ARGON_METHOD5(option_, get,
              "Returns the contained value."
              ""
              "If contained value is empty this method panic."
              ""
              "- Returns: Contained object."
              "- Panic ValueError: Option::get() on a empty value.", 0, false) {
    auto *opt = (Option *) self;

    if (opt->some == nullptr)
        return ErrorFormat(&error_value_error, "Option::get() on a empty value");

    return IncRef(opt->some);
}

ARGON_METHOD5(option_, get_or,
              "Returns the contained value or a provided default."
              ""
              "Default value are eagerly evaluated."
              ""
              "- Returns: Contained object or default value.", 1, false) {
    auto *opt = (Option *) self;

    if (opt->some == nullptr)
        return IncRef(*argv);

    return IncRef(opt->some);
}

const NativeFunc option_methods[] = {
        option_get_,
        option_get_or_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots option_obj = {
        option_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ArObject *option_ctor(ArObject **args, ArSize count) {
    if (!VariadicCheckPositional("option", count, 0, 1))
        return nullptr;

    if (count == 1)
        return OptionNew(*args);

    return OptionNew(nullptr);
}

void option_cleanup(Option *self) {
    Release(self->some);
}

ArObject *option_compare(Option *self, ArObject *other, CompareMode mode) {
    auto *o = (Option *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == other || self->some == o->some)
        return BoolToArBool(true);

    if (self->some != nullptr && o->some != nullptr)
        return RichCompare(self->some, o->some, CompareMode::EQ);

    return BoolToArBool(false);
}

bool option_is_true(Option *self) {
    return self->some != nullptr;
}

ArObject *option_str(Option *self) {
    return StringNewFormat("Option<%s>", self->some == nullptr ? "?" : AR_TYPE_NAME(self->some));
}

const TypeInfo argon::object::type_option_ = {
        TYPEINFO_STATIC_INIT,
        "option",
        nullptr,
        sizeof(Option),
        TypeInfoFlags::BASE,
        option_ctor,
        (VoidUnaryOp) option_cleanup,
        nullptr,
        (CompareOp) option_compare,
        (BoolUnaryOp) option_is_true,
        nullptr,
        (UnaryOp) option_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &option_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

Option *argon::object::OptionNew(ArObject *obj) {
    auto *opt = (Option *) ArObjectNew(RCType::INLINE, &type_option_);

    if (opt != nullptr)
        opt->some = IncRef(obj);

    return opt;
}