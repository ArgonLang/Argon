// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_PANIC_H_
#define ARGON_VM_PANIC_H_

#include <argon/vm/datatype/arobject.h>

namespace argon::vm {
    struct Panic {
        /// Prev panic.
        Panic *panic;

        /// Pointer to panic object.
        datatype::ArObject *object;

        /// If the panic originated in an Argon context it is the ID (raw pointer) of the frame that generated the error.
        uintptr_t gen_id;

        /// This panic was recovered?
        bool recovered;

        /// This panic was aborted? if so, a new panic has occurred during the management of this panic.
        bool aborted;
    };

    struct Panic *PanicNew(struct Panic *prev, datatype::ArObject *object);

} // namespace argon::vm

#endif // !ARGON_VM_PANIC_H_
