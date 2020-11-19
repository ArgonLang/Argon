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
        nullptr
};


Bounds *argon::object::BoundsNew(arsize start, arsize stop, arsize step) {
    auto bounds = ArObjectNew<Bounds>(RCType::INLINE, &type_bounds_);

    if (bounds != nullptr) {
        bounds->start = start;
        bounds->stop = stop;
        bounds->step = step;
    }

    return bounds;
}
