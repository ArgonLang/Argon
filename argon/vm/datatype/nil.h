// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_NIL_H_
#define ARGON_VM_DATATYPE_NIL_H_

#include <argon/vm/datatype/arobject.h>

#define ARGON_NIL_VALUE \
    (argon::vm::datatype::ArObject *)argon::vm::datatype::IncRef(argon::vm::datatype::Nil)

namespace argon::vm::datatype {
    struct NilBase {
        AROBJ_HEAD;

        bool value;
    };
    extern const TypeInfo *type_nil_;

    extern NilBase *Nil;

    /**
     * @brief Returns the object passed as an argument, or Nil if nullptr is passed.
     * @warning The reference of the passed object is not incremented.
     *
     * @param object Object to return or nullptr.
     * @return Object passed as an argument, or Nil if nullptr is passed.
     */
    inline ArObject *NilOrValue(ArObject *object) {
        if (object == nullptr)
            return (ArObject *) IncRef(Nil);

        return object;
    }
} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_NIL_H_
