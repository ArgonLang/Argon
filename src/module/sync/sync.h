// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULE_SYNC_H_
#define ARGON_MODULE_SYNC_H_

#include <object/arobject.h>

namespace argon::module::sync {
    extern const argon::object::TypeInfo *type_cond_;
    extern const argon::object::TypeInfo *type_locker_;
    extern const argon::object::TypeInfo *type_mutex_;
    extern const argon::object::TypeInfo *type_rwmutex_;
    extern const argon::object::TypeInfo *type_notifyqueue_;
} // namespace argon::module

#endif // !ARGON_MODULE_SYNC_H_
