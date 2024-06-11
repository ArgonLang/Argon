// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <shared_mutex>

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/nil.h>
#include <argon/vm/datatype/pcheck.h>
#include <argon/vm/datatype/tuple.h>

#include <argon/vm/datatype/chan.h>

using namespace argon::vm::datatype;

ARGON_FUNCTION(chan_chan, Chan,
               "Create a new Chan object.\n"
               "\n"
               "Default backlog: 1.\n"
               "\n"
               "- KWParameters:\n"
               "  - backlog: Set the size of the backlog.\n"
               "  - defval: Sets the value to be returned when a read operation is performed on a closed channel."
               "- returns: New Chan object.\n",
               nullptr, false, true) {
    ArObject *defval;

    IntegerUnderlying backlog;

    if (!KParamLookupInt((Dict *) kwargs, "backlog", &backlog, 1))
        return nullptr;

    if (!KParamLookup((Dict *) kwargs, "defval", nullptr, &defval, nullptr, false))
        return nullptr;

    if (backlog < 0) {
        ErrorFormat(kValueError[0], "backlog value cannot be negative");

        return nullptr;
    }

    return (ArObject *) ChanNew(defval, backlog);
}

ARGON_METHOD(chan_close, close,
             "Close this channel.\n"
             "\n"
             "The sender should be the only one to close the channel; multiple closures of a channel are considered a non-op.\n"
             "\n",
             nullptr, false, false) {
    auto self = (Chan *) _self;

    std::unique_lock _(self->lock);

    self->close = true;

    self->r_queue.NotifyAll();
    self->w_queue.NotifyAll();

    return (ArObject *) IncRef(Nil);
}

ARGON_METHOD(chan_flush, flush,
             "Empty the entire contents of the Chan.\n",
             nullptr, false, false) {
    auto self = (Chan *) _self;

    std::unique_lock _(self->lock);

    while (self->count > 0) {
        Release(self->queue[self->read]);

        self->read = (self->read + 1) % self->length;

        self->count--;
    }

    self->w_queue.NotifyAll();

    return (ArObject *) IncRef(Nil);
}

ARGON_METHOD(chan_isclosed, isclosed,
             "Test if this channel is closed.\n"
             "\n"
             "- Returns: True if channel is closed, false otherwise.\n",
             nullptr, false, false) {
    auto self = (Chan *) _self;
    return (ArObject *) BoolToArBool(self->close);
}

ARGON_METHOD(chan_read, read,
             "Read data from channel.\n"
             "\n"
             "- Returns: Tuple containing the value and a state indicating whether the value is reliable or not. If the state is false, "
             "the channel is closed, and the read value is invalid.",
             nullptr, false, false) {
    auto self = (Chan *) _self;

    ArObject *value;
    Tuple *ret;

    std::unique_lock _(self->lock);

    if (self->close && self->count == 0)
        return (ArObject *) TupleNew("ob", self->defval, false);

    if (!ChanRead(self, &value))
        return nullptr;

    ret = TupleNew("ob", value, true);

    Release(value);

    return (ArObject *) ret;
}

const FunctionDef chan_methods[] = {
        chan_chan,

        chan_close,
        chan_flush,
        chan_isclosed,
        chan_read,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots chan_objslot = {
        chan_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *chan_compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

ArObject *chan_repr(const Chan *self) {
    return (ArObject *) StringFormat("<%s -- backlog: %d, count: %d, closed: %s>", type_chan_->name, self->length,
                                     self->count, self->close ? "true" : "false");
}

bool chan_dtor(Chan *self) {
    while (self->count > 0) {
        Release(self->queue[self->read]);

        self->read = (self->read + 1) % self->length;

        self->count--;
    }

    self->lock.~RecursiveSharedMutex();

    self->r_queue.~NotifyQueue();
    self->w_queue.~NotifyQueue();

    argon::vm::memory::Free(self->queue);
    return true;
}

void chan_trace(Chan *self, Void_UnaryOp trace) {
    std::shared_lock _(self->lock);

    auto count = self->count;
    auto read = self->read;

    while (count > 0) {
        trace(self->queue[read]);

        read = (read + 1) % self->length;

        count--;
    }
}

TypeInfo ChanType = {
        AROBJ_HEAD_INIT_TYPE,
        "Chan",
        nullptr,
        nullptr,
        sizeof(Chan),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) chan_dtor,
        (TraceOp) chan_trace,
        nullptr,
        nullptr,
        chan_compare,
        (UnaryConstOp) chan_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &chan_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_chan_ = &ChanType;

bool argon::vm::datatype::ChanRead(Chan *chan, ArObject **out_value) {
    std::unique_lock _(chan->lock);

    if (chan->count == 0) {
        if (chan->close) {

            if (chan->defval == nullptr) {
                ErrorFormat(kRuntimeError[0], "read from closed channel");

                return false;
            }

            *out_value = IncRef(chan->defval);

            return true;
        }

        *out_value = nullptr;

        chan->r_queue.Wait(FiberStatus::BLOCKED_SUSPENDED);

        return false;
    }

    *out_value = chan->queue[chan->read];

    chan->count--;

    chan->read = (chan->read + 1) % chan->length;

    chan->w_queue.Notify();

    return true;
}

bool argon::vm::datatype::ChanWrite(Chan *chan, ArObject *value) {
    std::unique_lock _(chan->lock);

    if (chan->close) {
        ErrorFormat(kRuntimeError[0], "write on closed channel");
        return false;
    }

    if (chan->write == chan->read && chan->count > 0) {
        chan->w_queue.Wait(FiberStatus::BLOCKED_SUSPENDED);

        return false;
    }

    chan->queue[chan->write] = IncRef(value);

    chan->count++;

    chan->write = (chan->write + 1) % chan->length;

    chan->r_queue.Notify();

    return true;
}

Chan *argon::vm::datatype::ChanNew(ArObject *defval, unsigned int backlog) {
    auto *chan = MakeGCObject<Chan>(type_chan_);

    if (chan != nullptr) {
        if ((chan->queue = (ArObject **) argon::vm::memory::Alloc(backlog * sizeof(void *))) == nullptr) {
            Release(chan);

            return nullptr;
        }

        new(&chan->lock)sync::RecursiveSharedMutex();

        new(&chan->r_queue)sync::NotifyQueue();
        new(&chan->w_queue)sync::NotifyQueue();

        chan->defval = IncRef(defval);

        chan->read = 0;
        chan->write = 0;

        chan->count = 0;
        chan->length = backlog;

        chan->close = false;
    }

    return chan;
}
