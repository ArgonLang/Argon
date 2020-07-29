// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BOUNDS_H_
#define ARGON_OBJECT_BOUNDS_H_

#include <object/objmgmt.h>

namespace argon::object {

    struct Bounds : ArObject {
        arsize start;
        arsize stop;
        arsize step;
    };

    extern const TypeInfo type_bounds_;

    Bounds *BoundsNew(arsize start, arsize stop, arsize step);
} // namespace argon::object

#endif // !ARGON_OBJECT_BOUNDS_H_
