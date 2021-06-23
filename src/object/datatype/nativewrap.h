// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NATIVEWRAP_H_
#define ARGON_OBJECT_NATIVEWRAP_H_

#include <object/arobject.h>

namespace argon::object{
    struct NativeWrapper : ArObject {
        char *name;
        int offset;
        NativeMemberType mtype;
        bool readonly;
    };

    extern const TypeInfo *type_native_wrapper_;

    NativeWrapper *NativeWrapperNew(const NativeMember *member);

    ArObject *NativeWrapperGet(const NativeWrapper *wrapper, const ArObject *native);

    bool NativeWrapperSet(const NativeWrapper *wrapper, const ArObject *native, ArObject *value);

} // namespace argon::object

#endif // !ARGON_OBJECT_NATIVEWRAP_H_
