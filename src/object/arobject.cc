// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <vm/runtime.h>

#include <object/datatype/bool.h>
#include <object/datatype/error.h>
#include <object/datatype/function.h>
#include <object/datatype/nil.h>
#include <object/datatype/nativewrap.h>

#include "gc.h"
#include "arobject.h"

using namespace argon::object;

ArObject *MROSearch(const TypeInfo *type, ArObject *key, PropertyInfo *pinfo) {
    ArObject *obj = nullptr;

    if (type->mro != nullptr) {
        for (ArSize i = 0; i < ((Tuple *) type->mro)->len; i++) {
            auto *cursor = (TypeInfo *) ((Tuple *) type->mro)->objects[i];

            if (cursor->tp_map != nullptr) {
                if ((obj = NamespaceGetValue((Namespace *) cursor->tp_map, key, pinfo)) != nullptr)
                    break;
            }
        }
    }

    return obj;
}

ArObject *type_get_static_attr(ArObject *self, ArObject *key) {
    const TypeInfo *type = AR_GET_TYPEOBJ(self);
    const TypeInfo *tp_base = type;
    ArObject *instance = nullptr;
    ArObject *obj;

    PropertyInfo pinfo{};

    if (type->tp_map == nullptr && type->mro == nullptr)
        return ErrorFormat(type_attribute_error_, "type '%s' has no attributes", type->name);

    if (argon::vm::GetRoutine()->frame != nullptr)
        instance = argon::vm::GetRoutine()->frame->instance;

    if ((obj = NamespaceGetValue((Namespace *) type->tp_map, key, &pinfo)) == nullptr) {
        if (type->mro != nullptr)
            obj = MROSearch(type, key, &pinfo);

        if (obj == nullptr)
            return ErrorFormat(type_attribute_error_, "unknown attribute '%s' of object '%s'",
                               ((String *) key)->buffer, type->name);
    }

    if (!pinfo.IsConstant()) {
        ErrorFormat(type_access_violation_,
                    "in order to access to non const member '%s' an instance of '%s' is required",
                    ((String *) key)->buffer, type->name);
        Release(obj);
        return nullptr;
    }

    if (!pinfo.IsPublic() && !TraitIsImplemented(instance, tp_base)) {
        ErrorFormat(type_access_violation_, "access violation, member '%s' of '%s' are private",
                    ((String *) key)->buffer, type->name);
        Release(obj);
        return nullptr;
    }

    return obj;
}

ArObject *type_get_attr(ArObject *self, ArObject *key) {
    const TypeInfo *ancestor = AR_GET_TYPEOBJ(self);
    auto **ns = (Namespace **) AR_GET_NSOFF(self);
    ArObject *instance = nullptr;
    ArObject *obj = nullptr;

    PropertyInfo pinfo{};

    if (!AR_IS_TYPE_INSTANCE(self))
        return ErrorFormat(type_type_error_, "object is not an instance of type '%s'", ancestor->name);

    if (AR_OBJECT_SLOT(self) == nullptr)
        return ErrorFormat(type_attribute_error_, "object of type '%s' does not support attribute(.) operator",
                           ancestor->name);

    if (argon::vm::GetRoutine()->frame != nullptr)
        instance = argon::vm::GetRoutine()->frame->instance;

    if (AR_OBJECT_SLOT(self)->nsoffset >= 0)
        obj = NamespaceGetValue(*ns, key, &pinfo);

    if (obj == nullptr) {
        if (ancestor->tp_map != nullptr)
            obj = NamespaceGetValue((Namespace *) ancestor->tp_map, key, &pinfo);

        if (obj == nullptr && ancestor->mro != nullptr)
            obj = MROSearch(ancestor, key, &pinfo);
    }

    if (obj != nullptr) {
        if (!pinfo.IsPublic() && !TraitIsImplemented(instance, ancestor)) {
            ErrorFormat(type_access_violation_, "access violation, member '%s' of '%s' are private",
                        ((String *) key)->buffer, ancestor->name);
            Release(obj);
            return nullptr;
        }

        return obj;
    }

    return ErrorFormat(type_attribute_error_, "unknown attribute '%s' of instance '%s'",
                       ((String *) key)->buffer, ancestor->name);
}

bool type_set_attr(ArObject *obj, ArObject *key, ArObject *value) {
    auto **ns = (Namespace **) AR_GET_NSOFF(obj);
    ArObject *instance = nullptr;
    ArObject *actual;

    bool is_tpm = false;
    bool ok;

    PropertyInfo pinfo{};

    if (!AR_IS_TYPE_INSTANCE(obj))
        return ErrorFormat(type_type_error_, "object is not an instance of type '%s'", AR_TYPE_NAME(obj));

    if (AR_OBJECT_SLOT(obj) == nullptr)
        return ErrorFormat(type_attribute_error_, "object of type '%s' does not support attribute(.) operator",
                           AR_TYPE_NAME(obj));

    if (argon::vm::GetRoutine()->frame != nullptr)
        instance = argon::vm::GetRoutine()->frame->instance;

    if (AR_OBJECT_SLOT(obj)->nsoffset < 0) {
        ns = (Namespace **) &(AR_GET_TYPE(obj)->tp_map);
        is_tpm = true;
    }

    if ((actual = NamespaceGetValue(*ns, key, &pinfo)) == nullptr) {
        ErrorFormat(type_attribute_error_, "unknown attribute '%s' of instance '%s'",
                    ((String *) key)->buffer, AR_TYPE_NAME(obj));
        return false;
    }

    if (!pinfo.IsPublic() && !AR_SAME_TYPE(instance, obj)) {
        ErrorFormat(type_access_violation_, "access violation, member '%s' of '%s' are private",
                    ((String *) key)->buffer, AR_TYPE_NAME(obj));
        Release(actual);
        return false;
    }

    if (AR_TYPEOF(actual, type_native_wrapper_)) {
        ok = NativeWrapperSet((NativeWrapper *) actual, obj, value);
        Release(actual);
        return ok;
    }

    Release(actual);

    if (is_tpm) {
        ErrorFormat(type_unassignable_error_, "%s::%s is read-only", AR_TYPE_NAME(obj), ((String *) key)->buffer);
        return false;
    }

    return NamespaceSetValue(*ns, key, value);
}

const ObjectSlots type_obj = {
        nullptr,
        nullptr,
        nullptr,
        type_get_attr,
        type_get_static_attr,
        type_set_attr,
        nullptr,
        -1
};

bool type_is_true(ArObject *self) {
    return true;
}

ArObject *type_compare(ArObject *self, ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

ArSize type_hash(ArObject *self) {
    return (ArSize) self; // returns memory pointer as ArSize
}

ArObject *type_str(ArObject *self) {
    auto *tp = (TypeInfo *) self;

    switch (tp->flags) {
        case TypeInfoFlags::STRUCT:
            return StringNewFormat("<struct '%s'>", tp->name);
        case TypeInfoFlags::TRAIT:
            return StringNewFormat("<trait '%s'>", tp->name);
        default:
            return StringNewFormat("<datatype '%s'>", tp->name);
    }
}

void type_cleanup(TypeInfo *self) {
    argon::memory::Free((char *) self->name);
    Release(self->mro);
    Release(self->tp_map);
}

const TypeInfo TypeType = {
        TYPEINFO_STATIC_INIT,
        "datatype",
        nullptr,
        sizeof(TypeInfo),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) type_cleanup,
        nullptr,
        type_compare,
        type_is_true,
        type_hash,
        type_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &type_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_type_ = &TypeType;

const TypeInfo TraitType = {
        TYPEINFO_STATIC_INIT,
        "trait",
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
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_trait_ = &TraitType;

List *BuildBasesList(TypeInfo **types, ArSize count) {
    List *tmp = nullptr;
    List *bases;

    ArSize cap;

    if ((bases = ListNew(count)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < count; i++) {
        cap = 1;

        // Sanity check
        if (AR_GET_TYPE(types[i]) != type_type_) {
            // is not a TypeInfo
            goto error;
        }

        /*
        if(types[i]->flags != TypeInfoFlags::TRAIT){
            // you can only inherit from traits
            goto error;
        }
         */

        if (types[i]->mro != nullptr)
            cap += ((Tuple *) types[i]->mro)->len;

        if ((tmp = ListNew(cap)) == nullptr)
            goto error;

        /*
         * MRO list should contain the trait itself as the first element,
         * this would cause a circular reference!
         * To avoid this, the trait itself is excluded from the MRO list.
         *
         * To perform the calculation, however, it must be included!
         * Therefore it is added during the generation of the list of base traits.
         */
        if (!ListAppend(tmp, types[i]))
            goto error;
        // ******************************

        if (types[i]->mro != nullptr) {
            if (!ListConcat(tmp, types[i]->mro))
                goto error;
        }

        if (!ListAppend(bases, tmp))
            goto error;

        Release(tmp);
    }

    return bases;

    error:
    Release(tmp);
    Release(bases);
    return nullptr;
}

/*
 * Calculate MRO with C3 linearization
 * WARNING: This function uses Tuple object in raw mode!
 * NO IncRef or Release will be made during elaboration.
 * TODO: move from List to LinkedList or iterator!

 * T1  T2  T3  T4  T5  T6  T7  T8  T9  ...  TN
 * ^  ^                                       ^
 * |  +---------------------------------------+
 * |                   Tail
 * +--Head
 */
Tuple *ComputeMRO(List *bases) {
    ArSize hlist_idx = 0;
    Tuple *ret;
    List *output;

    if ((output = ListNew()) == nullptr)
        return nullptr;

    while (hlist_idx < bases->len) {
        // Get head list
        auto head_list = (List *) bases->objects[hlist_idx];

        if (head_list->len > 0) {
            // Get head of head_list
            auto head = head_list->objects[0];

            // Check if head is in the tail of any other lists
            for (ArSize i = 0; i < bases->len; i++) {
                if (hlist_idx != i) {
                    auto tail_list = ((List *) bases->objects[i]);

                    for (ArSize j = 1; j < tail_list->len; j++) {
                        auto target = (TypeInfo *) tail_list->objects[j];
                        if (head == target)
                            goto next_head;
                    }
                }
            }

            // If the current head is equal to head of another list, remove it!
            for (ArSize i = 0; i < bases->len; i++) {
                auto tail_list = ((List *) bases->objects[i]);

                if (hlist_idx != i) {
                    if (head == tail_list->objects[0])
                        ListRemove(tail_list, 0);
                }
            }

            if (!ListAppend(output, head)) {
                Release(output);
                return nullptr;
            }

            ListRemove(head_list, 0);
            hlist_idx = 0;
            continue;
        }

        next_head:
        hlist_idx++;
    }

    ret = TupleNew(output);
    Release(output);

    // if len(output) == 0 no good head was found (Ehm, yes this is a user error... warn him!)
    return ret;
}

bool CalculateMRO(TypeInfo *type, TypeInfo **bases, ArSize count) {
    auto *mro = (Tuple *) type->mro;
    List *merge = nullptr;
    List *bases_list;

    if (count == 0)
        return true;

    if (mro != nullptr) {
        if (mro->len > 0) {
            if ((merge = ListNew(mro->len + count)) == nullptr) {
                Release((ArObject **) type->mro);
                goto error;
            }

            ListConcat(merge, mro);

            for (ArSize i = 0; i < count; i++)
                ListAppend(merge, bases[i]);

            bases = (TypeInfo **) merge->objects;
            count = merge->len;
        }

        Release((ArObject **) type->mro);
    }

    if ((bases_list = BuildBasesList(bases, count)) == nullptr)
        goto error;

    type->mro = ComputeMRO(bases_list);
    Release(bases_list);

    error:
    Release(merge);
    return type->mro != nullptr;
}

ArObject *argon::object::ArObjectGCNew(const TypeInfo *type) {
    auto obj = (ArObject *) GCNew(type->size);

    if (obj != nullptr) {
        obj->ref_count = RefBits((unsigned char) RCType::GC);
        obj->type = IncRef((TypeInfo *) type);
    } else argon::vm::Panic(error_out_of_memory);

    return obj;
}

ArObject *argon::object::ArObjectGCNewTrack(const TypeInfo *type) {
    auto *obj = ArObjectGCNew(type);
    Track(obj);
    return obj;
}

ArObject *argon::object::ArObjectNew(RCType rc, const TypeInfo *type) {
    auto obj = (ArObject *) memory::Alloc(type->size);

    if (obj != nullptr) {
        obj->ref_count = RefBits((unsigned char) rc);
        obj->type = IncRef((TypeInfo *) type);
    } else argon::vm::Panic(error_out_of_memory);

    return obj;
}

ArObject *argon::object::InstanceGetMethod(const ArObject *instance, const ArObject *key, bool *is_meth) {
    ArObject *ret;

    if ((ret = PropertyGet(instance, key, true)) != nullptr) {
        if (is_meth != nullptr)
            *is_meth = AR_TYPEOF(ret, type_function_) && ((Function *) ret)->IsMethod();
    }

    return ret;
}

ArObject *argon::object::InstanceGetMethod(const ArObject *instance, const char *key, bool *is_meth) {
    ArObject *ret;
    String *akey;

    if ((akey = StringNew(key)) == nullptr)
        return nullptr;

    ret = InstanceGetMethod(instance, akey, is_meth);
    Release(akey);

    return ret;
}

ArObject *argon::object::IteratorGet(const ArObject *obj) {
    if (!IsIterable(obj))
        return ErrorFormat(type_type_error_, "'%s' is not iterable", AR_TYPE_NAME(obj));

    return AR_GET_TYPE(obj)->iter_get((ArObject *) obj);
}

ArObject *argon::object::IteratorGetReversed(const ArObject *obj) {
    if (!IsIterableReversed(obj))
        return ErrorFormat(type_type_error_, "'%s' is not reverse iterable", AR_TYPE_NAME(obj));

    return AR_GET_TYPE(obj)->iter_rget((ArObject *) obj);
}

ArObject *argon::object::IteratorNext(ArObject *iterator) {
    if (!IsIterator(iterator))
        return ErrorFormat(type_type_error_, "expected an iterator not '%s'", AR_TYPE_NAME(iterator));

    return AR_ITERATOR_SLOT(iterator)->next(iterator);
}

ArObject *argon::object::PropertyGet(const ArObject *obj, const ArObject *key, bool instance) {
    BinaryOp get_attr = type_type_->obj_actions->get_attr;
    BinaryOp get_sattr = type_type_->obj_actions->get_static_attr;
    ArObject *ret;
    ArObject *tmp;

    if (AR_OBJECT_SLOT(obj) != nullptr) {
        if (AR_OBJECT_SLOT(obj)->get_attr != nullptr)
            get_attr = AR_OBJECT_SLOT(obj)->get_attr;

        if (AR_OBJECT_SLOT(obj)->get_static_attr != nullptr)
            get_sattr = AR_OBJECT_SLOT(obj)->get_static_attr;
    }

    ret = instance ? get_attr((ArObject *) obj, (ArObject *) key) : get_sattr((ArObject *) obj, (ArObject *) key);

    if (ret != nullptr && AR_TYPEOF(ret, type_native_wrapper_)) {
        tmp = NativeWrapperGet((NativeWrapper *) ret, obj);
        Release(ret);
        ret = tmp;
    }

    return ret;
}

ArObject *argon::object::RichCompare(const ArObject *obj, const ArObject *other, CompareMode mode) {
    static const CompareMode reverse[] = {CompareMode::EQ, CompareMode::NE, CompareMode::LE,
                                          CompareMode::LEQ, CompareMode::GE, CompareMode::GEQ};
    static const char *str_mode[] = {"==", "!=", ">", ">=", "<", "<="};

    ArObject *result = nullptr;
    bool ne = false;

    if (mode == CompareMode::NE) {
        mode = CompareMode::EQ;
        ne = true;
    }

    if (AR_GET_TYPE(obj)->compare != nullptr)
        result = AR_GET_TYPE(obj)->compare((ArObject *) obj, (ArObject *) other, mode);

    if (result == nullptr && AR_GET_TYPE(other)->compare != nullptr)
        result = AR_GET_TYPE(other)->compare((ArObject *) other, (ArObject *) obj, reverse[(int) mode]);

    if (result == nullptr) {
        if (mode == CompareMode::EQ)
            return IncRef(False);

        return ErrorFormat(type_not_implemented_, "operator '%s' not supported between instance of '%s' and '%s'",
                           str_mode[(int) mode], AR_TYPE_NAME(obj), AR_TYPE_NAME(other));
    }

    if (ne) {
        ne = !ArBoolToBool((Bool *) result);
        Release(result);
        result = BoolToArBool(ne);
    }

    return result;
}

ArObject *argon::object::ToString(ArObject *obj) {
    if (AR_GET_TYPE(obj)->str != nullptr)
        return AR_GET_TYPE(obj)->str(obj);

    return StringNewFormat("<object %s @%p>", AR_TYPE_NAME(obj), obj);
    // return ErrorFormat(type_runtime_error_, "unimplemented slot 'str' for object '%s'", AR_TYPE_NAME(obj));
}

ArObject *argon::object::TypeNew(const TypeInfo *meta, const char *name, ArObject *ns, TypeInfo **bases, ArSize count) {
    ArSize name_len = strlen(name) + 1; // +1 '\0'
    TypeInfo *type;

    if (ns != nullptr && !AR_TYPEOF(ns, type_namespace_))
        return ErrorFormat(type_type_error_, "TypeNew expected Namespace at third parameter, not '%s'",
                           AR_TYPE_NAME(ns));

    if ((type = (TypeInfo *) memory::Alloc(sizeof(TypeInfo))) == nullptr) {
        argon::vm::Panic(error_out_of_memory);
        return nullptr;
    }

    argon::memory::MemoryCopy(type, meta, sizeof(TypeInfo));
    type->ref_count = RefBits((unsigned char) RCType::INLINE);
    type->type = IncRef((TypeInfo *) type_type_);

    type->name = (char *) argon::memory::Alloc(name_len);
    argon::memory::MemoryCopy((char *) type->name, name, name_len);

    if (count > 0) {
        if (!CalculateMRO(type, bases, count)) {
            Release(type);
            return nullptr;
        }
    }

    if (!TypeInit(type, ns))
        Release((ArObject **) type);

    return type;
}

ArObject *argon::object::TypeNew(const TypeInfo *meta, ArObject *name, ArObject *ns, TypeInfo **bases, ArSize count) {
    if (!AR_TYPEOF(name, type_string_))
        return ErrorFormat(type_type_error_, "TypeNew expected string as name, not '%s'",
                           AR_TYPE_NAME(name));

    return TypeNew(meta, (const char *) ((String *) name)->buffer, ns, bases, count);
}

ArSize argon::object::Hash(ArObject *obj) {
    if (IsHashable(obj))
        return AR_GET_TYPE(obj)->hash(obj);
    return 0;
}

ArSSize argon::object::Length(const ArObject *obj) {
    if (AsSequence(obj) && AR_GET_TYPE(obj)->sequence_actions->length != nullptr)
        return AR_GET_TYPE(obj)->sequence_actions->length((ArObject *) obj);
    else if (AsMap(obj) && AR_GET_TYPE(obj)->map_actions->length != nullptr)
        return AR_GET_TYPE(obj)->map_actions->length((ArObject *) obj);

    ErrorFormat(type_type_error_, "'%s' has no len", AR_TYPE_NAME(obj));
    return -1;
}

bool argon::object::BufferGet(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags) {
    if (!IsBufferable(obj)) {
        ErrorFormat(type_type_error_, "bytes-like object is required, not '%s'", obj->type->name);
        return false;
    }

    return obj->type->buffer_actions->get_buffer(obj, buffer, flags);
}

bool argon::object::BufferSimpleFill(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags, unsigned char *raw,
                                     ArSize len, bool writable) {
    if (buffer == nullptr) {
        ErrorFormat(type_buffer_error_, "bad call to BufferSimpleFill, buffer == nullptr");
        return false;
    }

    if (ENUMBITMASK_ISTRUE(flags, ArBufferFlags::WRITE) && !writable) {
        ErrorFormat(type_buffer_error_, "buffer of object '%s' is not writable", obj->type->name);
        return false;
    }

    buffer->buffer = raw;
    buffer->len = len;
    IncRef(obj);
    buffer->obj = obj;
    buffer->flags = flags;

    return true;
}

bool argon::object::Equal(const ArObject *obj, const ArObject *other) {
    ArObject *rich = RichCompare(obj, other, CompareMode::EQ);
    bool result = ArBoolToBool((Bool *) rich);

    Release(rich);
    return result;
}

bool argon::object::IsNull(const ArObject *obj) {
    if (obj == nullptr)
        return true;

    return AR_TYPEOF(obj, type_nil_);
}

bool argon::object::IsTrue(const ArObject *obj) {
    if (AR_GET_TYPE(obj)->is_true == nullptr)
        return false;

    return AR_GET_TYPE(obj)->is_true((ArObject *) obj);
}

bool argon::object::PropertySet(ArObject *obj, ArObject *key, ArObject *value, bool member) {
    BoolTernOp set_attr = type_type_->obj_actions->set_attr;
    BoolTernOp set_sattr = type_type_->obj_actions->set_static_attr;

    if (AR_OBJECT_SLOT(obj) != nullptr) {
        if (AR_OBJECT_SLOT(obj)->set_attr != nullptr)
            set_attr = AR_OBJECT_SLOT(obj)->set_attr;

        if (AR_OBJECT_SLOT(obj)->set_static_attr != nullptr)
            set_sattr = AR_OBJECT_SLOT(obj)->set_static_attr;
    }

    if (!member) {
        if (set_sattr == nullptr) {
            ErrorFormat(type_type_error_, "'%s' object is unable to set static member", AR_TYPE_NAME(obj));
            return false;
        }

        return set_sattr(obj, key, value);
    }

    return set_attr(obj, key, value);
}

bool argon::object::TraitIsImplemented(const ArObject *obj, const TypeInfo *type) {
    const TypeInfo *obj_type;

    if (obj == nullptr || type == nullptr)
        return false;

    obj_type = AR_GET_TYPE(obj);

    if (obj_type == type)
        return true;

    if (obj_type->mro == nullptr)
        return false;

    for (ArSize i = 0; i < ((Tuple *) obj_type->mro)->len; i++) {
        if (((Tuple *) obj_type->mro)->objects[i] == type)
            return true;
    }

    return false;
}

bool InitMembers(TypeInfo *info) {
    ArObject *tmp;

    if (info->obj_actions == nullptr)
        return true;

    // Functions / Methods
    if (info->obj_actions->methods != nullptr) {
        for (const NativeFunc *method = info->obj_actions->methods; method->name != nullptr; method++) {
            if ((tmp = FunctionNew(nullptr, info, method, method->method)) == nullptr)
                goto error;

            if (!NamespaceNewSymbol((Namespace *) info->tp_map, ((Function *) tmp)->name, tmp,
                                    PropertyType::PUBLIC | PropertyType::CONST))
                goto error;

            Release(tmp);
        }
    }

    // Members
    if (info->obj_actions->members != nullptr) {
        for (const NativeMember *member = info->obj_actions->members; member->name != nullptr; member++) {
            if ((tmp = NativeWrapperNew(member)) == nullptr)
                goto error;

            if (!NamespaceNewSymbol((Namespace *) info->tp_map, member->name, tmp,
                                    PropertyType::PUBLIC | PropertyType::CONST))
                goto error;

            Release(tmp);
        }
    }


    return true;

    error:
    Release(tmp);
    return false;
}

bool argon::object::TypeInit(TypeInfo *info, ArObject *ns) {
    static PropertyType meth_flags = PropertyType::PUBLIC | PropertyType::CONST;
    ArSize blen = 0;

    assert(info->tp_map == nullptr);

    if (ns == nullptr && (info->obj_actions == nullptr || info->obj_actions->methods == nullptr))
        return true;

    // Calculate static MRO
    if (info->obj_actions != nullptr && info->obj_actions->traits != nullptr) {
        // Count base traits
        for (const TypeInfo **base = info->obj_actions->traits; *base != nullptr; blen++, base++);

        if (!CalculateMRO(info, (TypeInfo **) info->obj_actions->traits, blen))
            return false;
    }

    // Build namespace
    info->tp_map = IncRef(ns);
    if (ns == nullptr && (info->tp_map = NamespaceNew()) == nullptr)
        goto error;

    // Push methods
    if (info->obj_actions != nullptr) {
        if (!InitMembers(info))
            goto error;
    }

    return true;

    error:
    Release(&info->mro);
    Release(&info->tp_map);
    return false;
}

bool argon::object::VariadicCheckPositional(const char *name, int nargs, int min, int max) {
    if (nargs < min) {
        ErrorFormat(type_type_error_, "%s expected %s%d argument%s, got %d", name, (min == max ? "" : "at least "),
                    min, min == 1 ? "" : "s", nargs);
        return false;
    } else if (nargs > max) {
        ErrorFormat(type_type_error_, "%s expected %s%d argument%s, got %d", name, (min == max ? "" : "at most "),
                    max, max == 1 ? "" : "s", nargs);
        return false;
    }

    return true;
}

int argon::object::TrackRecursive(ArObject *obj) {
    auto routine = vm::GetRoutine();
    List *references = routine->references;

    // search ref on list
    if (references->len > 0) {
        if (references->objects[references->len - 1] == obj)
            return 1;

        for (ArSize i = 0; i < references->len - 1; i++)
            if (references->objects[i] == obj)
                return 1;
    }

    // not found, push it!
    if (!ListAppend(references, obj))
        return -1;

    return 0;
}

void argon::object::UntrackRecursive(ArObject *obj) {
    auto routine = vm::GetRoutine();
    List *references = routine->references;

    assert(references->objects[references->len - 1] == obj);

    ListRemove(references, references->len - 1);
}

void argon::object::BufferRelease(ArBuffer *buffer) {
    if (buffer->obj == nullptr)
        return;

    if (buffer->obj->type->buffer_actions->rel_buffer != nullptr)
        buffer->obj->type->buffer_actions->rel_buffer(buffer);

    Release(&buffer->obj);
}

void argon::object::Release(ArObject *obj) {
    if (obj == nullptr)
        return;

    if (obj->ref_count.DecStrong()) {
        if (obj->ref_count.IsGcObject())
            return GCFree(obj);

        if (obj->type->cleanup != nullptr)
            obj->type->cleanup(obj);

        Release((ArObject *) obj->type);
        argon::memory::Free(obj);
    }
}
