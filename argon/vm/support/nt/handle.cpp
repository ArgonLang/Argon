// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <windows.h>

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/nil.h>
#include <argon/vm/datatype/pcheck.h>

#include <argon/vm/support/nt/handle.h>

using namespace argon::vm::datatype;
using namespace argon::vm::support::nt;

ARGON_FUNCTION(oshandle_handle, Handle,
               "Creates a new Handle object by specifying the value as integer.\n"
               "\n"
               "- Parameter handle: Integer representing a valid windows handle.\n"
               "- Returns: Handle object.\n",
               "iu: handle", false, false) {
    auto handle = (HANDLE) ((Integer *) args[0])->uint;
    unsigned long flags;

    if (GetHandleInformation(handle, &flags) == 0) {
        ErrorFromWinErr();

        return nullptr;
    }

    return (ArObject *) OSHandleNew(handle);
}

ARGON_METHOD(oshandle_waitobject, waitobject,
             "Waits until the specified object is in the signaled state or the time-out interval elapses.\n"
             "\n"
             "If no wait time is specified, the function waits until a change occurs in the observed object.\n"
             "\n"
             "- Parameter handle: A handle to the object.\n"
             "- KWParameters:\n"
             "  - wait: The time-out interval, in milliseconds.\n"
             "- Remarks: See Windows WaitForSingleObject function for more details.\n",
             nullptr, false, true) {
    auto *self = (OSHandle *) _self;
    IntegerUnderlying millisecond = INFINITE;

    if (!KParamLookupInt((Dict *) kwargs, "wait", &millisecond, INFINITE))
        return nullptr;

    auto ok = WaitForSingleObject(self->handle, millisecond);

    if (ok == WAIT_TIMEOUT) {
        ErrorFormat(kTimeoutError[0], "waitobject timed out before the specified object was signaled.");

        return nullptr;
    } else if (ok == WAIT_FAILED) {
        ErrorFromWinErr();

        return nullptr;
    }

    return (ArObject *) IncRef(Nil);
}

const FunctionDef oshandle_methods[] = {
        oshandle_handle,

        oshandle_waitobject,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots oshandle_objslot = {
        oshandle_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *oshandle_compare(const OSHandle *self, ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if ((ArObject *) self != other)
        return BoolToArBool(self->handle == ((OSHandle *) other)->handle);

    return BoolToArBool(true);
}

ArObject *oshandle_repr(const OSHandle *self) {
    if (self->handle == INVALID_HANDLE_VALUE)
        return (ArObject *) StringNew("<Handle: INVALID_HANDLE>");

    return (ArObject *) StringFormat("<Handle 0x%X>", self->handle);
}

bool oshandle_dtor(OSHandle *self) {
    CloseHandle(self->handle);

    return true;
}

TypeInfo OSHandleType = {
        AROBJ_HEAD_INIT_TYPE,
        "Handle",
        nullptr,
        nullptr,
        sizeof(OSHandle),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) oshandle_dtor,
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) oshandle_compare,
        (UnaryConstOp) oshandle_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &oshandle_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::support::nt::type_oshandle_ = &OSHandleType;

OSHandle *argon::vm::support::nt::OSHandleNew(HANDLE handle) {
    auto *hobj = MakeObject<OSHandle>(type_oshandle_);

    if (hobj != nullptr)
        hobj->handle = handle;

    return hobj;
}

#endif