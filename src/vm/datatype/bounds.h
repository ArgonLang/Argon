// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_BOUNDS_H_
#define ARGON_VM_BOUNDS_H_

#include "arobject.h"
#include "integer.h"

namespace argon::vm::datatype{
    struct Bounds {
        AROBJ_HEAD;

        Integer *start;
        Integer *stop;
        Integer *step;
    };
    extern const TypeInfo *type_bounds_;

    /**
     * @brief Retrieve the start, stop, and step indices from the bounds.
     *
     * @param bound Bounds object.
     * @param length Length of the slice.
     * @param *out_start Returns the starting point.
     * @param *out_stop Returns the ending point
     * @param *out_step Returns the size of the single step.
     * @return Distance between stop and start.
     */
    ArSSize BoundsIndex(Bounds *bounds, ArSize length, ArSSize *out_start, ArSSize *out_stop, ArSSize *out_step);

    /**
     * @brief Return a new bounds object with the given values.
     *
     * @param start start value.
     * @param stop stop value.
     * @param step increment value.
     * @return A pointer to the newly created bounds object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    Bounds *BoundsNew(ArObject *start, ArObject *stop, ArObject *step);

} // argon::vm::datatype

#endif // !ARGON_VM_BOUNDS_H_
