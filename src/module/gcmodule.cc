// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/gc.h>

#include <object/datatype/bool.h>
#include <object/datatype/error.h>
#include <object/datatype/integer.h>

#include "modules.h"

using namespace argon::object;
using namespace argon::module;

ARGON_FUNCTION(collect, "Run a collection on selected generation."
                        ""
                        "- Parameter generation: generation to be collected."
                        "- Returns: number of collected objects is returned.",
               1, false) {
    IntegerUnderlying gen;

    if (!AR_TYPEOF(argv[0], type_integer_))
        return ErrorFormat(type_type_error_, "collect expected integer as generation, not '%s'", AR_TYPE_NAME(argv[0]));

    gen = ((Integer *) argv[0])->integer;

    if (gen >= ARGON_OBJECT_GC_GENERATIONS)
        return ErrorFormat(type_value_error_, "unknown generation %d (from 0 to 2)", gen);

    return IntegerNew((IntegerUnderlying) STWCollect(gen));
}

ARGON_FUNCTION(collectfull, "Run a full collection."
                            ""
                            "- Returns: number of collected objects is returned.",
               0, false) {
    return IntegerNew((IntegerUnderlying) STWCollect());
}

ARGON_FUNCTION(getcount, "Returns number of strong reference to the object."
                         ""
                         "- Parameter obj: object to check."
                         "- Returns: strong reference counts.",
               1, false) {
    long ref = (long) argv[0]->ref_count.GetStrongCount();
    return IntegerNew(ref);
}

ARGON_FUNCTION(getweakcount, "Returns number of weak reference to the object."
                             ""
                             "- Parameter obj: object to check."
                             "- Returns: weak reference counts.", 1, false) {
    long ref = (long) argv[0]->ref_count.GetWeakCount();
    return IntegerNew(ref);
}

ARGON_FUNCTION(havesidetable, "Check if object have a SideTable."
                              ""
                              "- Parameter obj: object to check."
                              "- Returns: true if object have a SideTable, false otherwise.",
               1, false) {
    return BoolToArBool(argv[0]->ref_count.HaveSideTable());
}

ARGON_FUNCTION(isenabled, "Check if automatic collection is enabled."
                          ""
                          "- Returns: true if automatic collection is enabled, false otherwise.",
               0, false) {
    return BoolToArBool(GCIsEnabled());
}

ARGON_FUNCTION(isimmortal, "Check if object is immortal."
                           ""
                           "- Parameter obj: object to check."
                           "- Returns: true if object is immortal, false otherwise.",
               1, false) {
    return BoolToArBool(argv[0]->ref_count.IsStatic());
}

ARGON_FUNCTION(istracked, "Check if object is tracked by GC."
                          ""
                          "- Parameter obj: object to check."
                          "- Returns: true if object is tracked by GC, false otherwise.",
               1, false) {
    return BoolToArBool(GCIsTracking(argv[0]));
}

const PropertyBulk gc_bulk[] = {
        MODULE_EXPORT_FUNCTION(collect_),
        MODULE_EXPORT_FUNCTION(collectfull_),
        MODULE_EXPORT_FUNCTION(getcount_),
        MODULE_EXPORT_FUNCTION(getweakcount_),
        MODULE_EXPORT_FUNCTION(havesidetable_),
        MODULE_EXPORT_FUNCTION(isenabled_),
        MODULE_EXPORT_FUNCTION(isimmortal_),
        MODULE_EXPORT_FUNCTION(istracked_),
        MODULE_EXPORT_SENTINEL
};

const ModuleInit module_gc = {
        "gc",
        "The GC module provides access to GC functionality and "
        "provides information on the status of objects managed by the ARC.",
        gc_bulk,
        nullptr,
        nullptr
};
const ModuleInit *argon::module::module_gc_ = &module_gc;
