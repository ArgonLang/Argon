// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "arstring.h"
#include "boolean.h"
#include "error.h"
#include "function.h"
#include "namespace.h"
#include "nativewrapper.h"
#include "nil.h"

#include "arobject.h"

using namespace argon::vm::datatype;

static List *static_references = nullptr;

// Prototypes

ArObject *MROSearch(const TypeInfo *type, ArObject *key, AttributeProperty *aprop);

List *BuildBasesList(TypeInfo **bases, unsigned int length);

Tuple *CalculateMRO(const List *bases);

// ***

ArObject *type_compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

ArObject *type_get_attr(const ArObject *self, ArObject *key, bool static_attr) {
    const TypeInfo *ancestor = AR_GET_TYPE(self);
    auto **ns = (Namespace **) AR_GET_NSOFFSET(self);
    const ArObject *instance = nullptr;
    ArObject *ret = nullptr;

    AttributeProperty aprop{};

    if (!AR_HAVE_OBJECT_BEHAVIOUR(self)) {
        ErrorFormat(kAttributeError[0], static_attr ? kAttributeError[2] : kAttributeError[1], ancestor->name);
        return nullptr;
    }

    if (static_attr && !AR_TYPEOF(self, type_type_)) {
        ErrorFormat(kTypeError[0], kTypeError[1], AR_TYPE_NAME(self));
        return nullptr;
    }

    const auto *frame = argon::vm::GetFrame();
    if (frame != nullptr)
        instance = frame->instance;

    if (!static_attr) {
        if (AR_SLOT_OBJECT(self)->namespace_offset >= 0)
            ret = NamespaceLookup(*ns, key, &aprop);

        if (ret == nullptr) {
            if (ancestor->tp_map != nullptr)
                ret = NamespaceLookup((Namespace *) ancestor->tp_map, key, &aprop);

            if (ret == nullptr && ancestor->mro != nullptr)
                ret = MROSearch(ancestor, key, &aprop);
        }
    } else
        ret = NamespaceLookup((Namespace *) ((const TypeInfo *) self)->tp_map, key, &aprop);

    if (ret == nullptr) {
        ErrorFormat(kAttributeError[0], kAttributeError[3], ARGON_RAW_STRING((String *) key), ancestor->name);
        return nullptr;
    }

    if (static_attr && !aprop.IsConstant()) {
        ErrorFormat(kAccessViolationError[0], kAccessViolationError[2],
                    ARGON_RAW_STRING((String *) key), ancestor->name);
        Release(&ret);
    }

    if (!aprop.IsPublic() && !TraitIsImplemented(instance, ancestor)) {
        ErrorFormat(kAccessViolationError[0], kAccessViolationError[1],
                    ARGON_RAW_STRING((String *) key), ancestor->name);
        Release(&ret);
    }

    if (AR_TYPEOF(ret, type_native_wrapper_)) {
        auto *value = NativeWrapperGet((NativeWrapper *) ret, self);

        Release(ret);

        return value;
    }

    return ret;
}

ArObject *type_repr(const TypeInfo *self) {
    return nullptr;
}

ArSize type_hash(TypeInfo *self) {
    return (ArSize) self; // returns memory pointer as ArSize
}

bool type_dtor(TypeInfo *self) {
    /*
     * NB: Destructor is only called on dynamically generated types,
     * in fact it will never be called on basic types such as atom, bytes, decimal, etc.
     */
    argon::vm::memory::Free((void *) self->name);
    argon::vm::memory::Free((void *) self->qname);
    argon::vm::memory::Free((void *) self->doc);

    Release(self->mro);
    Release(self->tp_map);

    return true;
}

bool type_set_attr(ArObject *self, ArObject *key, ArObject *value, bool static_attr) {
    auto **ns = (Namespace **) AR_GET_NSOFFSET(self);
    const ArObject *instance = nullptr;
    ArObject *current = nullptr;

    bool is_tpm = false;

    AttributeProperty aprop{};

    if (!AR_HAVE_OBJECT_BEHAVIOUR(self)) {
        ErrorFormat(kAttributeError[0], static_attr ? kAttributeError[2] : kAttributeError[1], AR_TYPE_NAME(self));
        return false;
    }

    const auto *frame = argon::vm::GetFrame();
    if (frame != nullptr)
        instance = frame->instance;

    if (AR_SLOT_OBJECT(self)->namespace_offset < 0) {
        ns = (Namespace **) &(AR_GET_TYPE(self)->tp_map);
        is_tpm = true;
    }

    if ((current = NamespaceLookup(*ns, key, &aprop)) == nullptr) {
        ErrorFormat(kAttributeError[0], kAttributeError[3], AR_TYPE_NAME(self));
        return false;
    }

    if (!aprop.IsPublic() && (instance == nullptr || !AR_SAME_TYPE(instance, self))) {
        ErrorFormat(kAccessViolationError[0], kAccessViolationError[1],
                    ARGON_RAW_STRING((String *) key), AR_TYPE_NAME(self));

        Release(current);
        return false;
    }

    if (AR_TYPEOF(current, type_native_wrapper_)) {
        auto ok = NativeWrapperSet((NativeWrapper *) current, self, value);

        Release(current);

        return ok;
    }

    Release(current);

    if (is_tpm || aprop.IsConstant()) {
        ErrorFormat(kUnassignableError[0], kUnassignableError[2],
                    AR_TYPE_QNAME(self), ARGON_RAW_STRING((String *) key));

        return false;
    }

    return NamespaceSet(*ns, key, value);
}

const ObjectSlots type_objslot = {
        nullptr,
        nullptr,
        nullptr,
        type_get_attr,
        type_set_attr,
        -1
};

TypeInfo TypeType = {
        AROBJ_HEAD_INIT_TYPE,
        "Type",
        nullptr,
        nullptr,
        0,
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) type_dtor,
        nullptr,
        (ArSize_UnaryOp) type_hash,
        nullptr,
        type_compare,
        (UnaryConstOp) type_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &type_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_type_ = &TypeType;

const TypeInfo TypeTrait = {
        AROBJ_HEAD_INIT_TYPE,
        "Trait",
        nullptr,
        nullptr,
        0,
        TypeInfoFlags::TRAIT,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ArObject *argon::vm::datatype::AttributeLoad(const ArObject *object, ArObject *key, bool static_attr) {
    AttributeGetter aload = type_type_->object->get_attr;

    if (AR_HAVE_OBJECT_BEHAVIOUR(object) && AR_SLOT_OBJECT(object)->get_attr != nullptr)
        aload = AR_SLOT_OBJECT(object)->get_attr;

    return aload(object, key, static_attr);
}

ArObject *argon::vm::datatype::AttributeLoadMethod(const ArObject *object, ArObject *key, bool *is_method) {
    ArObject *aload;

    *is_method = false;

    if ((aload = AttributeLoad(object, key, false)) == nullptr)
        return nullptr;

    if (AR_TYPEOF(aload, type_function_) && ((Function *) aload)->IsMethod())
        *is_method = true;

    return aload;
}

ArObject *argon::vm::datatype::ComputeMRO(TypeInfo *type, TypeInfo **bases, unsigned int length) {
    const auto *mro = (const Tuple *) type->mro;
    TypeInfo **merge = nullptr;
    Tuple *ret = nullptr;
    List *bases_list;

    if (length == 0)
        return (ArObject *) TupleNew((ArSize) 0);

    if (mro != nullptr) {
        if (mro->length > 0) {
            if ((merge = (TypeInfo **) memory::Alloc((mro->length + length) * sizeof(void *))) == nullptr)
                return nullptr;

            int count = 0;

            for (ArSize i = 0; i < mro->length; i++)
                merge[count++] = (TypeInfo *) IncRef(mro->objects[i]);

            for (ArSize i = 0; i < length; i++)
                merge[count++] = IncRef(bases[i]);

            bases = merge;
            length = count;
        }

        Release(&type->mro);
    }

    if ((bases_list = BuildBasesList(bases, length)) != nullptr) {
        ret = CalculateMRO(bases_list);
        Release(bases_list);
    }

    if (merge != nullptr) {
        for (int i = 0; i < length; i++)
            Release(merge[i]);

        memory::Free(merge);
    }

    return (ArObject *) ret;
}

ArObject *argon::vm::datatype::Compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    static const CompareMode reverse[] = {CompareMode::EQ, CompareMode::NE, CompareMode::LE,
                                          CompareMode::LEQ, CompareMode::GR, CompareMode::GRQ};
    static const char *str_mode[] = {"==", "!=", ">", ">=", "<", "<="};
    ArObject *result = nullptr;
    auto lc = AR_GET_TYPE(self)->compare;
    auto rc = AR_GET_TYPE(other)->compare;
    bool ne = false;

    if (mode == CompareMode::NE) {
        mode = CompareMode::EQ;
        ne = true;
    }

    if (lc != nullptr)
        result = lc(self, other, mode);

    if (result == nullptr && rc != nullptr)
        result = rc(other, self, reverse[(int) mode]);

    if (result == nullptr) {
        if (mode != CompareMode::EQ) {
            ErrorFormat(kNotImplementedError[0], kNotImplementedError[2], str_mode[(int) mode],
                        AR_TYPE_NAME(self), AR_TYPE_NAME(other));
            return nullptr;
        }

        result = (ArObject *) IncRef(False);
    }

    if (ne) {
        ne = !ArBoolToBool((Boolean *) result);
        Release(result);
        result = BoolToArBool(ne);
    }

    return result;
}

ArObject *argon::vm::datatype::IteratorGet(ArObject *object, bool reversed) {
    if (AR_GET_TYPE(object)->iter == nullptr) {
        ErrorFormat(kTypeError[0], "'%s' is not iterable", AR_TYPE_NAME(object));
        return nullptr;
    }

    return AR_GET_TYPE(object)->iter(object, reversed);
}

ArObject *argon::vm::datatype::IteratorNext(ArObject *iterator) {
    if (AR_GET_TYPE(iterator)->iter == nullptr) {
        ErrorFormat(kTypeError[0], "expected an iterator not '%s'", AR_TYPE_NAME(iterator));
        return nullptr;
    }

    return AR_GET_TYPE(iterator)->iter_next(iterator);
}

ArObject *MROSearch(const TypeInfo *type, ArObject *key, AttributeProperty *aprop) {
    if (type->mro == nullptr)
        return nullptr;

    const auto *mro = (Tuple *) type->mro;
    for (ArSize i = 0; i < mro->length; i++) {
        const auto *cursor = (const TypeInfo *) mro->objects[i];

        if (cursor->tp_map != nullptr) {
            auto *ret = NamespaceLookup((Namespace *) cursor->tp_map, key, aprop);
            if (ret != nullptr)
                return ret;
        }
    }

    return nullptr;
}

ArObject *argon::vm::datatype::Repr(const ArObject *object) {
    auto repr = AR_GET_TYPE(object)->repr;

    if (repr != nullptr)
        return repr(object);

    return (ArObject *) StringFormat("<object %s @%p>", AR_TYPE_NAME(object), object);
}

ArObject *argon::vm::datatype::Str(ArObject *object) {
    auto str = AR_GET_TYPE(object)->str;

    if (str != nullptr)
        return str(object);

    return Repr(object);
}

ArObject *argon::vm::datatype::TraitNew(const char *name, const char *qname, const char *doc, ArObject *ns,
                                        TypeInfo **bases, unsigned int length) {
    return TypeNew(&TypeTrait, name, qname, doc, ns, bases, length);
}

ArObject *argon::vm::datatype::TypeNew(const TypeInfo *type, const char *name, const char *qname, const char *doc,
                                       ArObject *ns, TypeInfo **bases, unsigned int length) {
    TypeInfo *ret;

    ArSize name_len;
    ArSize qname_len;
    ArSize doc_len;

    name_len = strlen(name);

    if (qname != nullptr)
        qname_len = strlen(qname);

    if (doc != nullptr)
        doc_len = strlen(doc);

    if ((ret = (TypeInfo *) memory::Calloc(sizeof(TypeInfo))) == nullptr)
        return nullptr;

    memory::MemoryCopy(ret, type, sizeof(TypeInfo));

    AR_GET_RC(ret) = memory::RCType::INLINE;
    AR_GET_TYPE(ret) = IncRef((TypeInfo *) type_type_);

    if ((ret->name = (char *) memory::Alloc(name_len + 1)) == nullptr) {
        memory::Free(ret);
        return nullptr;
    }

    memory::MemoryCopy((char *) ret->name, name, name_len);
    ((char *) ret->name)[name_len] = '\0';

    if (qname != nullptr) {
        ret->qname = (char *) memory::Alloc(qname_len + 1);
        if (ret->qname == nullptr) {
            memory::Free((void *) ret->name);

            memory::Free(ret);
            return nullptr;
        }

        memory::MemoryCopy((char *) ret->qname, qname, qname_len);
        ((char *) ret->qname)[qname_len] = '\0';
    }

    if (doc != nullptr && doc_len > 0) {
        ret->doc = (char *) memory::Alloc(doc_len + 1);
        if (ret->doc == nullptr) {
            memory::Free((void *) ret->name);
            memory::Free((void *) ret->qname);

            memory::Free(ret);
            return nullptr;
        }

        memory::MemoryCopy((char *) ret->doc, doc, doc_len);
        ((char *) ret->doc)[doc_len] = '\0';
    }

    if ((ret->mro = ComputeMRO(ret, bases, length)) == nullptr) {
        memory::Free((void *) ret->name);
        memory::Free((void *) ret->qname);
        memory::Free((void *) ret->doc);

        memory::Free(ret);
        return nullptr;
    }

    if (!TypeInit(ret, (ArObject *) ns))
        Release((ArObject **) ret);

    return (ArObject *) ret;
}

List *BuildBasesList(TypeInfo **bases, unsigned int length) {
    List *tmp = nullptr;
    List *ret;

    if ((ret = ListNew(length)) == nullptr)
        return nullptr;

    for (int i = 0; i < length; i++) {
        auto *type = bases[i];

        // Sanity check
        if (!AR_TYPEOF(type, type_type_)) {
            ErrorFormat(kTypeError[0], "you can only inherit from traits and '%s' is not", AR_GET_TYPE(type)->name);
            goto ERROR;
        }

        if (ENUMBITMASK_ISFALSE(type->flags, TypeInfoFlags::TRAIT)) {
            // you can only inherit from traits
            ErrorFormat(kTypeError[0], "you can only inherit from traits and '%s' is not", type->name);
            goto ERROR;
        }

        ArSize cap = 1;

        if (type->mro != nullptr)
            cap += ((Tuple *) type)->length;

        if ((tmp = ListNew(cap)) == nullptr)
            goto ERROR;

        /*
         * MRO list should contain the trait itself as the first element,
         * this would cause a circular reference!
         * To avoid this, the trait itself is excluded from the MRO list.
         *
         * To perform the calculation, however, it must be included!
         * Therefore, it is added during the generation of the list of base traits.
         */

        ListAppend(tmp, (ArObject *) type);

        // ***

        if (type->mro != nullptr)
            ListExtend(tmp, type->mro);

        ListAppend(ret, (ArObject *) tmp);

        Release((ArObject **) &tmp);
    }

    return ret;

    ERROR:
    Release(tmp);
    Release(ret);
    return nullptr;
}

Tuple *CalculateMRO(const List *bases) {
    /*
     * Calculate MRO with C3 Linearization
     * WARNING: This function uses List object in raw mode!
     * NO IncRef or Release will be made during elaboration.
     *
     * T1  T2  T3  T4  T5  T6  T7  T8  T9  ...  TN
     * ^  ^                                       ^
     * |  +---------------------------------------+
     * |                   Tail
     * +--Head
     */

    ArSize bases_idx = 0;
    List *output;
    Tuple *ret;

    if ((output = ListNew()) == nullptr)
        return nullptr;

    while (bases_idx < bases->length) {
        // Get head list
        auto head_list = (List *) bases->objects[bases_idx];

        if (head_list->length == 0) {
            bases_idx++;
            continue;
        }

        ArObject *head = head_list->objects[0];
        bool found = false;

        // Check if head is in the tail of any other list
        for (ArSize i = 0; i < bases->length && !found; i++) {
            if (bases_idx == i)
                continue;

            const auto *tail_list = (const List *) bases->objects[i];

            for (ArSize j = 1; j < tail_list->length; j++) {
                if (head == tail_list->objects[j]) {
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            bases_idx++;
            continue;
        }

        // If the current head is equal to head of another list, REMOVE IT!
        for (ArSize i = 0; i < bases->length; i++) {
            auto tail_list = ((List *) bases->objects[i]);

            if (bases_idx != i && head == tail_list->objects[0])
                ListRemove(tail_list, 0);
        }

        if (!ListAppend(output, head)) {
            Release(output);
            return nullptr;
        }

        ListRemove(head_list, 0);
        bases_idx = 0;
    }

    ret = TupleConvertList(&output);

    Release(output);

    return ret;
}

bool argon::vm::datatype::AttributeSet(ArObject *object, ArObject *key, ArObject *value, bool static_attr) {
    AttributeWriter awrite = type_type_->object->set_attr;

    if (AR_HAVE_OBJECT_BEHAVIOUR(object) && AR_SLOT_OBJECT(object)->set_attr != nullptr)
        awrite = AR_SLOT_OBJECT(object)->set_attr;

    return awrite(object, key, value, static_attr);
}

bool argon::vm::datatype::BufferGet(ArObject *object, ArBuffer *buffer, BufferFlags flags) {
    const auto *buf_slot = AR_GET_TYPE(object)->buffer;

    if (buf_slot == nullptr || buf_slot->get_buffer == nullptr) {
        ErrorFormat(kTypeError[0], "bytes-like object is required, not '%s'", AR_TYPE_NAME(object));
        return false;
    }

    auto ok = buf_slot->get_buffer(object, buffer, flags);

    if (ok)
        buffer->object = IncRef(object);

    return ok;
}

bool argon::vm::datatype::BufferSimpleFill(const ArObject *object, ArBuffer *buffer, BufferFlags flags,
                                           unsigned char *raw, ArSize item_size, ArSize nelem, bool writable) {
    if (buffer == nullptr) {
        ErrorFormat(kTypeError[0], "bad call to BufferSimpleFill, buffer == nullptr");
        return false;
    }

    if (ENUMBITMASK_ISTRUE(flags, BufferFlags::WRITE) && !writable) {
        ErrorFormat(kBufferError[0], kBufferError[1], AR_TYPE_NAME(object));
        return false;
    }

    buffer->buffer = raw;
    buffer->object = nullptr; // Filled by BufferGet
    buffer->geometry.item_size = item_size;
    buffer->geometry.nelem = nelem;
    buffer->length = item_size * nelem;
    buffer->flags = flags;

    return true;
}

bool argon::vm::datatype::Equal(const ArObject *self, const ArObject *other) {
    auto *cmp = Compare(self, other, CompareMode::EQ);
    bool result = ArBoolToBool((Boolean *) cmp);

    Release(cmp);
    return result;
}

bool argon::vm::datatype::Hash(ArObject *object, ArSize *out_hash) {
    auto hash = AR_GET_TYPE(object)->hash;

    if (hash != nullptr) {
        if (out_hash != nullptr) {
            *out_hash = hash(object);

            if (vm::IsPanicking())
                return false;
        }

        return true;
    }

    return false;
}

bool argon::vm::datatype::IsNull(const ArObject *object) {
    return object == nullptr || object == (ArObject *) Nil;
}

bool argon::vm::datatype::IsTrue(const ArObject *object) {
    if (AR_GET_TYPE(object)->is_true == nullptr)
        return true;

    return AR_GET_TYPE(object)->is_true(object);
}

bool InitMembers(TypeInfo *type) {
    auto *ns = (Namespace *) type->tp_map;

    if (type->object == nullptr)
        return true;

    // Function/Method
    if (type->object->methods != nullptr) {
        for (const FunctionDef *cursor = type->object->methods; cursor->name != nullptr; cursor++) {
            auto *fn = (ArObject *) FunctionNew(cursor, type, nullptr);
            if (fn == nullptr)
                return false;

            if (!NamespaceNewSymbol(ns, cursor->name, fn, AttributeFlag::CONST | AttributeFlag::PUBLIC)) {
                Release(fn);
                return false;
            }

            Release(fn);
        }
    }

    // TODO members

    return true;
}

bool argon::vm::datatype::TypeInit(TypeInfo *type, ArObject *auxiliary) {
    if (ENUMBITMASK_ISTRUE(type->flags, TypeInfoFlags::INITIALIZED))
        return true;

    assert(type->tp_map == nullptr);

    // TODO: traits here

    // Build tp_map
    type->tp_map = IncRef(auxiliary);
    if (auxiliary == nullptr) {
        type->tp_map = (ArObject *) NamespaceNew();
        if (type->tp_map == nullptr)
            return false;
    }

    // Push members
    if (!InitMembers(type)) {
        Release(&type->tp_map);
        return false;
    }

    // TODO: Override check!

    if (type->qname == nullptr) {
        type->qname = type->name;

        if (!type->head_.ref_count_.IsStatic()) {
            auto qn_len = strlen(type->name);

            auto *qname = (char *) vm::memory::Alloc(qn_len + 1);
            if (qname == nullptr) {
                Release(&type->tp_map);
                return false;
            }

            vm::memory::MemoryCopy(qname, type->name, qn_len);
            qname[qn_len] = '\0';

            type->qname = qname;
        }
    }

    *((TypeInfoFlags *) &type->flags) = type->flags | TypeInfoFlags::INITIALIZED;

    return true;
}

bool argon::vm::datatype::TraitIsImplemented(const ArObject *object, const TypeInfo *type) {
    const TypeInfo *obj_type;

    if (object == nullptr || type == nullptr)
        return false;

    obj_type = AR_GET_TYPE(object);

    if (obj_type == type)
        return true;

    if (obj_type->mro == nullptr)
        return false;

    const auto *mro = (Tuple *) obj_type->mro;
    for (ArSize i = 0; i < mro->length; i++) {
        if ((const TypeInfo *) mro->objects[i] == type)
            return true;
    }

    return false;
}

int argon::vm::datatype::RecursionTrack(ArObject *object) {
    auto *fiber = argon::vm::GetFiber();
    List **ref = &static_references;

    if (fiber != nullptr)
        ref = &fiber->references;

    if (*ref == nullptr) {
        *ref = ListNew();
        if (*ref == nullptr)
            return -1;
    }

    // search ref on list
    if ((*ref)->length > 0) {
        for (ArSize i = (*ref)->length; i > 0; i--)
            if ((*ref)->objects[i - 1] == object)
                return 1;
    }

    // not found, push it!
    if (!ListAppend(*ref, object))
        return -1;

    return 0;
}

void argon::vm::datatype::BufferRelease(ArBuffer *buffer) {
    if (buffer->object == nullptr)
        return;

    if (AR_GET_TYPE(buffer->object)->buffer->rel_buffer != nullptr)
        AR_GET_TYPE(buffer->object)->buffer->rel_buffer(buffer);

    Release(&buffer->object);
}

void argon::vm::datatype::Release(ArObject *object) {
    if (object == nullptr)
        return;

    if (AR_GET_RC(object).DecStrong()) {
        if (AR_GET_RC(object).IsGcObject()) {
            memory::GCFree(object);
            return;
        }

        if (AR_GET_TYPE(object)->dtor != nullptr)
            AR_GET_TYPE(object)->dtor(object);

        argon::vm::memory::Free(object);
    }
}

void argon::vm::datatype::RecursionUntrack(ArObject *object) {
    auto *fiber = argon::vm::GetFiber();
    List **ref = &static_references;

    if (fiber != nullptr)
        ref = &fiber->references;

    assert((*ref) != nullptr && (*ref)->objects[(*ref)->length - 1] == object);

    ListRemove(*ref, ((ArSSize) (*ref)->length) - 1);
}
