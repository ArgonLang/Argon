// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>

#include <module/modules.h>

#include "sync.h"

using namespace argon::object;
using namespace argon::module;
using namespace argon::module::sync;

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

    if (!ModuleAddProperty(self, "Cond", (ArObject *) type_cond_, MODULE_ATTRIBUTE_PUB_CONST))
        return false;

    if (!ModuleAddProperty(self, "Locker", (ArObject *) type_locker_, MODULE_ATTRIBUTE_PUB_CONST))
        return false;

    if (!ModuleAddProperty(self, "Mutex", (ArObject *) type_mutex_, MODULE_ATTRIBUTE_PUB_CONST))
        return false;

    if (!ModuleAddProperty(self, "RWMutex", (ArObject *) type_rwmutex_, MODULE_ATTRIBUTE_PUB_CONST))
        return false;

    if (!ModuleAddProperty(self, "NotifyQueue", (ArObject *) type_notifyqueue_, MODULE_ATTRIBUTE_PUB_CONST))
        return false;

    return true;
}

const ModuleInit module_sync = {
        "_sync",
        "This module provides basic synchronization primitives such as mutual exclusion locks. If you are looking "
        "for advance sync features, you should import sync, not _sync!",
        nullptr,
        sync_init,
        nullptr
};
const argon::object::ModuleInit *argon::module::module_sync_ = &module_sync;
