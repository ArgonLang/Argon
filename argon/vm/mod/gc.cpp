// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/memory/gc.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/function.h>
#include <argon/vm/datatype/integer.h>

#include <argon/vm/mod/modules.h>

using namespace argon::vm::datatype;

ARGON_FUNCTION(gc_collect, collect,
               "Run a collection on selected generation.\n"
               "\n"
               "- Parameter generation: Generation to be collected.\n"
               "- Returns: Number of collected objects is returned.\n",
               "iu: generation", false, false) {
    UIntegerUnderlying gen = ((Integer *) *args)->uint;

    if (AR_TYPEOF(*args, type_int_)) {
        if (((Integer *) *args)->sint < 0) {
            ErrorFormat(kValueError[0], "%s expected positive integer",
                        ARGON_RAW_STRING((String *) ((Function *) _func)->qname));

            return nullptr;
        }

        gen = ((Integer *) *args)->sint;
    }

    if (gen >= argon::vm::memory::kGCGenerations) {
        ErrorFormat(kValueError[0], "unknown generation %d (from 0 to %d)", gen, argon::vm::memory::kGCGenerations);
        return nullptr;
    }

    return (ArObject *) IntNew((IntegerUnderlying) argon::vm::memory::Collect((short) gen));
}

ARGON_FUNCTION(gc_collectall, collectall,
               "Run a full collection.\n"
               "\n"
               "- Returns: Number of collected objects is returned.\n",
               nullptr, false, false) {
    return (ArObject *) IntNew((IntegerUnderlying) argon::vm::memory::Collect());
}

ARGON_FUNCTION(gc_disable, disable,
               "Disable automatic garbage collection.\n"
               "\n"
               "- Returns: GC status before this call.\n",
               nullptr, false, false) {
    return BoolToArBool(argon::vm::memory::GCEnable(false));
}

ARGON_FUNCTION(gc_enable, enable,
               "Enable automatic garbage collection.\n"
               "\n"
               "- Returns: GC status before this call.\n",
               nullptr, false, false) {
    return BoolToArBool(argon::vm::memory::GCEnable(true));
}

ARGON_FUNCTION(gc_havesidetable, havesidetable,
               "Check if object have a SideTable.\n"
               "\n"
               "- Parameter obj: Object to check.\n"
               "- Returns: True if object have a SideTable, false otherwise.\n",
               ": obj", false, false) {
    return BoolToArBool(AR_GET_RC(*args).HaveSideTable());
}

ARGON_FUNCTION(gc_isenabled, isenabled,
               "Check if automatic collection is enabled.\n"
               "\n"
               "- Returns: True if automatic collection is enabled, false otherwise.\n",
               nullptr, false, false) {
    return BoolToArBool(argon::vm::memory::GCIsEnabled());
}

ARGON_FUNCTION(gc_isimmortal, isimmortal,
               "Check if object is immortal.\n"
               "\n"
               "- Parameter obj: Object to check.\n"
               "- Returns: True if object is immortal, false otherwise.\n",
               ": obj", false, false) {
    return BoolToArBool(AR_GET_RC(*args).IsStatic());
}

ARGON_FUNCTION(gc_istracked, istracked,
               "Check if object is tracked by GC.\n"
               "\n"
               "- Parameter obj: Object to check.\n"
               "- Returns: True if object is tracked by GC, false otherwise.\n",
               ": obj", false, false) {
    const auto *head = argon::vm::memory::GCGetHead(*args);

    if (head == nullptr)
        return BoolToArBool(false);

    return BoolToArBool(head->IsTracked());
}

ARGON_FUNCTION(gc_strongcount, strongcount,
               "Returns number of strong reference to the object.\n"
               "\n"
               "- Parameter obj: Object to check.\n"
               "- Returns: Strong reference counts.\n",
               ": obj", false, false) {
    return (ArObject *) UIntNew(AR_GET_RC(*args).GetStrongCount());
}

ARGON_FUNCTION(gc_weakcount, weakcount,
               "Returns number of weak reference to the object.\n"
               "\n"
               "- Parameter obj: Object to check.\n"
               "- Returns: Weak reference counts.\n",
               ": obj", false, false) {
    return (ArObject *) UIntNew(AR_GET_RC(*args).GetWeakCount());
}

const ModuleEntry gc_entries[] = {
        MODULE_EXPORT_FUNCTION(gc_collect),
        MODULE_EXPORT_FUNCTION(gc_collectall),
        MODULE_EXPORT_FUNCTION(gc_disable),
        MODULE_EXPORT_FUNCTION(gc_enable),
        MODULE_EXPORT_FUNCTION(gc_havesidetable),
        MODULE_EXPORT_FUNCTION(gc_isenabled),
        MODULE_EXPORT_FUNCTION(gc_isimmortal),
        MODULE_EXPORT_FUNCTION(gc_istracked),
        MODULE_EXPORT_FUNCTION(gc_strongcount),
        MODULE_EXPORT_FUNCTION(gc_weakcount),


        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleGC = {
        "gc",
        "The GC module provides access to GC functionality and "
        "provides information on the status of objects managed by the ARC.",
        nullptr,
        gc_entries,
        nullptr,
        nullptr
};
const ModuleInit *argon::vm::mod::module_gc_ = &ModuleGC;