// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>

#include <module/modules.h>

#include "sync.h"

using namespace argon::object;
using namespace argon::module;
using namespace argon::module::sync;

const PropertyBulk sync_bulk[] = {
        MODULE_EXPORT_TYPE_ALIAS("Cond", type_cond_),
        MODULE_EXPORT_TYPE_ALIAS("Locker", type_locker_),
        MODULE_EXPORT_TYPE_ALIAS("Mutex", type_mutex_),
        MODULE_EXPORT_TYPE_ALIAS("RWMutex", type_rwmutex_),
        MODULE_EXPORT_TYPE_ALIAS("NotifyQueue", type_notifyqueue_),
        MODULE_EXPORT_SENTINEL
};

bool sync_init(Module *self) {
    if (!TypeInit((TypeInfo *) type_locker_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_cond_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_mutex_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_rwmutex_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_notifyqueue_, nullptr))
        return false;

    return true;
}

const ModuleInit module_sync = {
        "_sync",
        "This module provides basic synchronization primitives such as mutual exclusion locks. If you are looking "
        "for advance sync features, you should import sync, not _sync!",
        sync_bulk,
        sync_init,
        nullptr
};
const argon::object::ModuleInit *argon::module::module_sync_ = &module_sync;
