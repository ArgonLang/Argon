// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_BOOLEAN_H_
#define ARGON_VM_DATATYPE_BOOLEAN_H_

#include <argon/vm/datatype/arobject.h>

namespace argon::vm::datatype {
    struct Boolean {
        AROBJ_HEAD;

        bool value;
    };
    _ARGONAPI extern const TypeInfo *type_boolean_;

    _ARGONAPI extern Boolean *True;
    _ARGONAPI extern Boolean *False;

    /**
     * @brief Converts Argon Boolean to C++ bool.
     * @param boolean Argon Boolean.
     * @return true or false.
     */
    inline bool ArBoolToBool(const Boolean *boolean) {
        return boolean->value;
    }

    /**
     * @brief Converts C++ bool to Argon Boolean.
     * @param value true or false.
     * @return An Argon Boolean True or False.
     */
    inline ArObject *BoolToArBool(bool value) {
        return (ArObject *) (value ? True : False);
    }

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_BOOLEAN_H_
