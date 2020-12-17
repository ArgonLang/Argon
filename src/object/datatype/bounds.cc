// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bounds.h"

using namespace argon::object;
using namespace argon::memory;

const TypeInfo argon::object::type_bounds_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "bounds",
        sizeof(Bounds),
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


Bounds *argon::object::BoundsNew(ArSSize start, ArSSize stop, ArSSize step) {
    auto bounds = ArObjectNew<Bounds>(RCType::INLINE, &type_bounds_);

    if (bounds != nullptr) {
        bounds->start = start;
        bounds->stop = stop;
        bounds->step = step;
    }

    return bounds;
}

ArSSize argon::object::BoundsIndex(Bounds *bound, size_t length, ArSSize *start, ArSSize *stop, ArSSize *step) {
    *start = bound->start;
    *stop = bound->stop;
    *step = bound->step;

    assert(bound->step != 0);

    // TODO: check overflow ?!

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