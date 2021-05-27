// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BOUNDS_H_
#define ARGON_OBJECT_BOUNDS_H_

#include <object/arobject.h>

#include "integer.h"

namespace argon::object {

    struct Bounds : ArObject {
        Integer *start;
        Integer *stop;
        Integer *step;
    };

    extern const TypeInfo *type_bounds_;

    Bounds *BoundsNew(ArObject *start, ArObject *stop, ArObject *step);

    ArSSize BoundsIndex(Bounds *bound, ArSize length, ArSSize *start, ArSSize *stop, ArSSize *step);

} // namespace argon::object

#endif // !ARGON_OBJECT_BOUNDS_H_
