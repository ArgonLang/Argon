// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/bool.h>
#include <object/datatype/integer.h>

#include "modules.h"

using namespace argon::object;
using namespace argon::module;

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
    return BoolToArBool(false); // TODO: fix this after enabled support to GC
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
    return BoolToArBool(argv[0]->ref_count.IsGcObject());
}

const PropertyBulk gc_bulk[] = {
        MODULE_EXPORT_FUNCTION(getcount_),
        MODULE_EXPORT_FUNCTION(getweakcount_),
        MODULE_EXPORT_FUNCTION(havesidetable_),
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
