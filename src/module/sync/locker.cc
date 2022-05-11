// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/nil.h>

#include "sync.h"

using namespace argon::object;

ARGON_METHOD5(locker_, lock, "", 0, false) {
    return IncRef(NilVal);
}

ARGON_METHOD5(locker_, unlock, "", 0, false) {
    return IncRef(NilVal);
}

const NativeFunc locker_methods[] = {
        locker_lock_,
        locker_unlock_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots locker_obj = {
        locker_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

const TypeInfo LockerTraitType = {
        TYPEINFO_STATIC_INIT,
        "Locker",
        nullptr,
        0,
        TypeInfoFlags::TRAIT,
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
        nullptr,
        nullptr,
        nullptr,
        &locker_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::sync::type_locker_ = &LockerTraitType;
