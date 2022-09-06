// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"
#include "boolean.h"

#include "arobject.h"

using namespace argon::vm::datatype;

ArObject *argon::vm::datatype::Compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    static const CompareMode reverse[] = {CompareMode::EQ, CompareMode::NE, CompareMode::LE,
                                          CompareMode::LEQ, CompareMode::GR, CompareMode::GRQ};
    static const char *str_mode[] = {"==", "!=", ">", ">=", "<", "<="};
    ArObject *result = nullptr;
    auto lc = AR_GET_TYPE(self)->compare;
    auto rc = AR_GET_TYPE(other)->compare;
    bool ne = false;

    if (mode == CompareMode::NE) {
        mode = CompareMode::EQ;
        ne = true;
    }

    if (lc != nullptr)
        result = lc(self, other, mode);

    if (result == nullptr && rc != nullptr)
        result = rc(other, self, reverse[(int) mode]);

    if (result == nullptr) {
        /* TODO: ERROR
        if (mode != CompareMode::EQ)
            return ErrorFormat(type_not_implemented_, "operator '%s' not supported between instance of '%s' and '%s'",
                               str_mode[(int) mode], AR_TYPE_NAME(obj), AR_TYPE_NAME(other));
                               */
        result = (ArObject *) IncRef(False);
    }

    if (ne) {
        ne = !ArBoolToBool((Boolean *) result);
        Release(result);
        result = BoolToArBool(ne);
    }

    return result;
}

ArObject *argon::vm::datatype::Repr(const ArObject *object) {
    auto repr = AR_GET_TYPE(object)->repr;

    if (repr != nullptr)
        return repr(object);

    return (ArObject *) StringFormat("<object %s @%p>", AR_TYPE_NAME(object), object);
}

ArObject *argon::vm::datatype::Str(const ArObject *object) {
    auto str = AR_GET_TYPE(object)->str;

    if (str != nullptr)
        return str(object);

    return Repr(object);
}

bool argon::vm::datatype::Equal(const ArObject *self, const ArObject *other) {
    auto *cmp = Compare(self, other, CompareMode::EQ);
    bool result = ArBoolToBool((Boolean *) cmp);

    Release(cmp);
    return result;
}

ArSize argon::vm::datatype::Hash(ArObject *object) {
    auto hash = AR_GET_TYPE(object)->hash;

    if (hash != nullptr)
        return hash(object);

    return 0;
}

void argon::vm::datatype::Release(ArObject *object) {
    // TODO: TMP IMPL!
    if(object== nullptr)
        return;

    if (AR_GET_RC(object).DecStrong()) {
        if (AR_GET_TYPE(object)->dtor != nullptr)
            AR_GET_TYPE(object)->dtor(object);

        argon::vm::memory::Free(object);
    }
}

