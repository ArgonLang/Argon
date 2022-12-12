// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"
#include "boolean.h"
#include "error.h"

#include "bounds.h"

using namespace argon::vm::datatype;

ArObject *bounds_compare(const Bounds *self, const Bounds *other, CompareMode mode) {
    const auto *o = other;
    bool val = false;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    switch (mode) {
        case CompareMode::EQ:
            val = self->start == o->start && self->stop == o->stop && self->step == o->step;
            break;
        case CompareMode::GR:
            val = self->start > o->start && self->stop > o->stop && self->step > o->step;
            break;
        case CompareMode::GRQ:
            val = self->start >= o->start && self->stop >= o->stop && self->step >= o->step;
            break;
        case CompareMode::LE:
            val = self->start < o->start && self->stop < o->stop && self->step < o->step;
            break;
        case CompareMode::LEQ:
            val = self->start <= o->start && self->stop <= o->stop && self->step <= o->step;
            break;
        default:
            assert(false);
    }

    return BoolToArBool(val);
}

ArObject *bounds_str(const Bounds *self) {
    return (ArObject *) StringFormat("bounds(%i, %i, %i)", self->start, self->stop, self->step);
}

bool bounds_dtor(Bounds *self) {
    Release(self->start);
    Release(self->stop);
    Release(self->step);

    return true;
}

TypeInfo BoundsType = {
        AROBJ_HEAD_INIT_TYPE,
        "Bounds",
        nullptr,
        nullptr,
        sizeof(Bounds),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) bounds_dtor,
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) bounds_compare,
        nullptr,
        (UnaryOp) bounds_str,
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
const TypeInfo *argon::vm::datatype::type_bounds_ = &BoundsType;

ArSSize argon::vm::datatype::BoundsIndex(Bounds *bound, ArSize length, ArSSize *out_start, ArSSize *out_stop,
                                         ArSSize *out_step) {
    ArSSize low;
    ArSSize high;

    *out_step = IsNull((ArObject *) bound->step) ? 1 : bound->step->sint;
    if (*out_step < 0) {
        low = -1;
        high = (ArSSize) length + low;
    } else {
        low = 0;
        high = (ArSSize) length;
    }

    *out_start = *out_step < 0 ? high : low;
    *out_stop = *out_step < 0 ? low : high;

    if (!IsNull((ArObject *) bound->start)) {
        *out_start = bound->start->sint;
        if (*out_start < 0) {
            if ((*out_start = *out_start + (ArSSize) length) < low)
                *out_start = low;
        } else if (*out_start > high)
            *out_start = high;
    }

    if (!IsNull((ArObject *) bound->stop)) {
        *out_stop = bound->stop->sint;
        if (*out_stop < 0) {
            if ((*out_stop = *out_stop + (ArSSize) length) < low)
                *out_stop = low;
        } else if (*out_stop > high)
            *out_stop = high;
    }

    // LENGTH
    if (*out_step < 0) {
        if (*out_stop < *out_start)
            return (*out_start - *out_stop - 1) / (-*out_step) + 1;
    } else if (*out_start < *out_stop)
        return (*out_stop - *out_start - 1) / *out_step + 1;

    return 0;
}

Bounds *argon::vm::datatype::BoundsNew(ArObject *start, ArObject *stop, ArObject *step) {
    if (!IsNull(step) && !AR_TYPEOF(step, type_int_)) {
        ErrorFormat(kTypeError[0], "step parameter must be '%s' not '%s'", type_int_->name, AR_TYPE_NAME(step));
        return nullptr;
    }

    if (!IsNull(stop) && !AR_TYPEOF(stop, type_int_)) {
        ErrorFormat(kTypeError[0], "stop parameter must be '%s' not '%s'", type_int_->name, AR_TYPE_NAME(stop));
        return nullptr;
    }

    if (!IsNull(start) && !AR_TYPEOF(start, type_int_)) {
        ErrorFormat(kTypeError[0], "start parameter must be '%s' not '%s'", type_int_->name, AR_TYPE_NAME(start));
        return nullptr;
    }

    auto *bound = MakeObject<Bounds>(type_bounds_);

    if (bound != nullptr) {
        bound->start = (Integer *) IncRef(start);
        bound->stop = (Integer *) IncRef(stop);
        bound->step = (Integer *) IncRef(step);
    }

    return bound;
}
