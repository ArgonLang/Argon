// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_NATIVEWRAPPER_H_
#define ARGON_VM_DATATYPE_NATIVEWRAPPER_H_

#include <argon/vm/datatype/arobject.h>

namespace argon::vm::datatype {
    struct NativeWrapper {
        AROBJ_HEAD;

        MemberDef member;
    };
    _ARGONAPI extern const TypeInfo *type_native_wrapper_;

    /**
     * @brief Convert a native C type to an Argon type.
     *
     * @param wrapper Pointer to NativeWrapper.
     * @param native Pointer to the struct from which to extract the native type.
     * @return An Argon type describing the native C type on success, otherwise returns nullptr and sets the panic state.
     */
    ArObject *NativeWrapperGet(const NativeWrapper *wrapper, const ArObject *native);

    /**
     * @param wrapper Pointer to NativeWrapper.
     *
     * @param native Pointer to the structure where the native type will be set.
     * @param value Pointer to Argon type which describe the value to be set.
     * @return True in case of success, otherwise returns false and sets the panic state.
     */
    bool NativeWrapperSet(const NativeWrapper *wrapper, ArObject *native, ArObject *value);

    /**
     * @brief Create a new NativeWrapper for a specific member of a struct.
     *
     * @param member Pointer to MemberDef that describes the native member to Argon.
     * @return A pointer to the newly created NativeWrapper object is returned,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    NativeWrapper *NativeWrapperNew(const MemberDef *member);
} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_NATIVEWRAPPER_H_
