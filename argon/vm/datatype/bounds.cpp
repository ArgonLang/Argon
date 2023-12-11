// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/error.h>

#include <argon/vm/datatype/bounds.h>

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

    ArSSize start;
    ArSSize stop;
    ArSSize step;

    step = IsNull(bound->step) ? 1 : ((Integer *) bound->step)->sint;
    if (step < 0) {
        low = -1;
        high = (ArSSize) length + low;
    } else {
        low = 0;
        high = (ArSSize) length;
    }

    start = step < 0 ? high : low;
    stop = step < 0 ? low : high;

    if (!IsNull(bound->start)) {
        start = ((Integer *) bound->start)->sint;
        if (start < 0) {
            if ((start = start + (ArSSize) length) < low)
                start = low;
        } else if (start > high)
            start = high;
    }

    if (!IsNull(bound->stop)) {
        stop = ((Integer *) bound->stop)->sint;
        if (stop < 0) {
            if ((stop = stop + (ArSSize) length) < low)
                stop = low;
        } else if (stop > high)
            stop = high;
    }

    *out_start = start;
    *out_stop = stop;
    *out_step = step;

    // LENGTH
    if (step < 0) {
        if (stop < start)
            return (start - stop - 1) / (-step) + 1;
    } else if (start < stop)
        return (stop - start - 1) / step + 1;

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
        bound->start = IncRef(start);
        bound->stop = IncRef(stop);
        bound->step = IncRef(step);
    }

    return bound;
}
