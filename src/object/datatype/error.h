// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_ERROR_H_
#define ARGON_OBJECT_ERROR_H_

#include <object/objmgmt.h>

namespace argon::object {

    struct Error : ArObject {
        ArObject *obj;
    };

    struct ErrorStr : ArObject {
        const char *msg;
    };

    Error *ErrorNew(ArObject *obj);

    inline ArObject *ReturnError(ArObject *err) {
        IncRef(err);
        return err;
    }

    // ExportedErrors
    extern ArObject *ZeroDivisionError;
    extern ArObject *OutOfMemoryError;
    extern ArObject *NotImplementedError;

} // namespace argon::object

#endif // !ARGON_OBJECT_ERROR_H_
