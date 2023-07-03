// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_PANIC_H_
#define ARGON_VM_PANIC_H_

#include <argon/vm/datatype/arobject.h>

#include <argon/vm/frame.h>

namespace argon::vm {
    struct Panic {
        /// Prev panic.
        Panic *panic;

        /// If the panic originated in an Argon context this is the pointer to frame that generated the error.
        Frame *frame;

        /// Pointer to panic object.
        datatype::ArObject *object;

        /// This panic was recovered?
        bool recovered;

        /// This panic was aborted? if so, a new panic has occurred during the management of this panic.
        bool aborted;
    };

    struct Panic *PanicNew(struct Panic *prev, Frame *frame, datatype::ArObject *object);

    inline struct Panic *PanicNew(struct Panic *prev, datatype::ArObject *object) {
        return PanicNew(prev, nullptr, object);
    }

    void PanicFill(struct Panic *panic, struct Panic *prev, Frame *frame, datatype::ArObject *object);

} // namespace argon::vm

#endif // !ARGON_VM_PANIC_H_
