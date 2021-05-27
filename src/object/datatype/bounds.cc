// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "error.h"
#include "bool.h"
#include "string.h"

#include "bounds.h"

using namespace argon::object;
using namespace argon::memory;

bool bounds_is_true(ArObject *self) {
    return true;
}

ArObject *bounds_compare(Bounds *self, ArObject *other, CompareMode mode) {
    auto *o = (Bounds *) other;
    bool val;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    switch (mode) {
        case CompareMode::EQ:
            val = self->start == o->start && self->stop == o->stop && self->step == o->step;
            break;
        case CompareMode::GE:
            val = self->start > o->start && self->stop > o->stop && self->step > o->step;
            break;
        case CompareMode::GEQ:
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

ArSize bounds_hash(ArObject *obj) {
    return 0;
}

ArObject *bounds_str(Bounds *self) {
    return StringNewFormat("bounds(%i,%i,%i)", self->start, self->stop, self->step);
}

const TypeInfo BoundsType = {
        TYPEINFO_STATIC_INIT,
        "bounds",
        nullptr,
        sizeof(Bounds),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) bounds_compare,
        bounds_is_true,
        bounds_hash,
        (UnaryOp) bounds_str,
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
const TypeInfo *argon::object::type_bounds_ = &BoundsType;

Bounds *argon::object::BoundsNew(ArObject *start, ArObject *stop, ArObject *step) {
    Bounds *bounds;

    if (!IsNull(step)) {
        if (!AsIndex(step))
            return (Bounds *) ErrorFormat(type_type_error_, "step parameter must be integer not '%s'",
                                          step->type->name);
    }

    if (!IsNull(stop)) {
        if (!AsIndex(stop))
            return (Bounds *) ErrorFormat(type_type_error_, "stop parameter must be integer not '%s'",
                                          step->type->name);
    }

    if (!IsNull(start)) {
        if (!AsIndex(start))
            return (Bounds *) ErrorFormat(type_type_error_, "start parameter must be integer not '%s'",
                                          start->type->name);
    }

    if ((bounds = ArObjectNew<Bounds>(RCType::INLINE, type_bounds_)) != nullptr) {
        bounds->start = (Integer *) IncRef(start);
        bounds->stop = (Integer *) IncRef(stop);
        bounds->step = (Integer *) IncRef(step);
    }

    return bounds;
}

ArSSize argon::object::BoundsIndex(Bounds *bound, ArSize length, ArSSize *start, ArSSize *stop, ArSSize *step) {
    ArSSize low;
    ArSSize high;

    *step = IsNull(bound->step) ? 1 : bound->step->integer;
    if (*step < 0) {
        low = -1;
        high = length + low;
    } else {
        low = 0;
        high = length;
    }

    *start = *step < 0 ? high : low;
    *stop = *step < 0 ? low : high;

    if (!IsNull(bound->start)) {
        *start = bound->start->integer;
        if (*start < 0) {
            if ((*start = *start + length) < low)
                *start = low;
        } else if (*start > high)
            *start = high;
    }

    if (!IsNull(bound->stop)) {
        *stop = bound->stop->integer;
        if (*stop < 0) {
            if ((*stop = *stop + length) < low)
                *stop = low;
        } else if (*stop > high)
            *stop = high;
    }


    // LENGTH
    if (*step < 0) {
        if (*stop < *start)
            return (*start - *stop - 1) / (-*step) + 1;
    } else if (*start < *stop)
        return (*stop - *start - 1) / *step + 1;

    return 0;
}