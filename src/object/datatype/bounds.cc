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

bool bounds_equal(Bounds *self, ArObject *other) {
    auto *o = (Bounds *) other;

    if (self == other)
        return true;

    return AR_SAME_TYPE(self, other)
           && self->start == o->start
           && self->stop == o->stop
           && self->step == o->step;
}

ArObject *bounds_compare(Bounds *self, ArObject *other, CompareMode mode) {
    auto *o = (Bounds *) other;
    bool val;

    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    switch (mode) {
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

size_t bounds_hash(ArObject *obj) {
    return 0;
}

ArObject *bounds_str(Bounds *self) {
    return StringNewFormat("bounds(%i,%i,%i)", self->start, self->stop, self->step);
}

const TypeInfo argon::object::type_bounds_ = {
        TYPEINFO_STATIC_INIT,
        "bounds",
        nullptr,
        sizeof(Bounds),
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) bounds_compare,
        (BoolBinOp) bounds_equal,
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
        nullptr
};

Bounds *argon::object::BoundsNew(ArSSize start, ArSSize stop, ArSSize step) {
    Bounds *bounds;

    if (step == 0)
        return (Bounds *) ErrorFormat(&error_value_error, "slice step cannot be 0");

    if ((bounds = ArObjectNew<Bounds>(RCType::INLINE, &type_bounds_)) != nullptr) {
        bounds->start = start;
        bounds->stop = stop;
        bounds->step = step;
    }

    return bounds;
}

Bounds *argon::object::BoundsNew(ArObject *start, ArObject *stop, ArObject *step) {
    ArSSize _step = 1;
    ArSSize _stop = 0;
    ArSSize _start;

    if (step != nullptr) {
        if (!AsIndex(step))
            return (Bounds *) ErrorFormat(&error_type_error, "step parameter must be integer not '%s'",
                                          step->type->name);
        _step = step->type->number_actions->as_index(step);
    }

    if (stop != nullptr) {
        if (!AsIndex(stop))
            return (Bounds *) ErrorFormat(&error_type_error, "stop parameter must be integer not '%s'",
                                          step->type->name);
        _stop = stop->type->number_actions->as_index(stop);
    }

    if (!AsIndex(start))
        return (Bounds *) ErrorFormat(&error_type_error, "start parameter must be integer not '%s'",
                                      start->type->name);
    _start = start->type->number_actions->as_index(start);

    return BoundsNew(_start, _stop, _step);
}

ArSSize argon::object::BoundsIndex(Bounds *bound, size_t length, ArSSize *start, ArSSize *stop, ArSSize *step) {
    *start = bound->start;
    *stop = bound->stop;
    *step = bound->step;

    // START
    if (bound->start < 0) {
        if ((*start += length) < 0)
            *start = bound->step < 0 ? -1 : 0;
    } else if (bound->start >= length)
        *start = bound->step < 0 ? length - 1 : length;

    // STOP
    if (bound->stop < 0) {
        if ((*stop += length) < 0)
            *stop = bound->step < 0 ? -1 : 0;
    } else if (bound->stop >= length)
        *stop = bound->step < 0 ? length - 1 : length;

    // LENGTH
    if (bound->step < 0) {
        if (*stop < *start)
            return (*start - *stop - 1) / (-bound->step) + 1;
    } else {
        if (*start < *stop)
            return (*stop - *start - 1) / bound->step + 1;
    }

    return 0;
}