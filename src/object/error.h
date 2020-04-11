// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_ERROR_H_
#define ARGON_OBJECT_ERROR_H_

#include "object.h"

namespace argon::object {
    struct NotImplemented : ArObject {
    };

    extern ArObject *NotImpl;

    inline ArObject *ReturnError(ArObject *err) {
        IncRef(err);
        return err;
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_ERROR_H_
