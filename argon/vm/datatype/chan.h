// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_CHAN_H_
#define ARGON_VM_DATATYPE_CHAN_H_

#include <argon/vm/sync/rsm.h>
#include <argon/vm/sync/ticket.h>

#include <argon/vm/datatype/arobject.h>

namespace argon::vm::datatype {
    struct Chan {
        AROBJ_HEAD;

        sync::RecursiveSharedMutex lock;

        sync::NotifyQueue r_queue;
        sync::NotifyQueue w_queue;

        ArObject **queue;

        unsigned int read;

        unsigned int write;

        unsigned int count;

        unsigned int length;
    };
    extern const TypeInfo *type_chan_;

    bool ChanRead(Chan *chan, ArObject **out_value);

    bool ChanWrite(Chan *chan, ArObject *value);

    Chan *ChanNew(unsigned int backlog);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_CHAN_H_
