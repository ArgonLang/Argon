// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "trait.h"
#include "tuple.h"
#include "error.h"

using namespace argon::object;
using namespace argon::memory;

ArObject *trait_get_static_attr(Trait *self, ArObject *key) {
    PropertyInfo pinfo{};
    ArObject *obj;

    obj = NamespaceGetValue(self->names, key, &pinfo);

    if (!pinfo.IsPublic()) {
        ErrorFormat(&error_access_violation, "access violation, member '%s' of '%s' are private",
                    ((String *) key)->buffer, "name");
        Release(obj);
        return nullptr;
    }

    return obj;
}

const ObjectSlots trait_actions{
        nullptr,
        nullptr,
        (BinaryOp) trait_get_static_attr,
        nullptr,
        nullptr
};

void trait_cleanup(Trait *self) {
    //Release(self->name);
    Release(self->names);
    Release(self->mro);
}

const TypeInfo argon::object::type_trait_ = {
        TYPEINFO_STATIC_INIT,
        "trait",
        nullptr,
        sizeof(Trait),
        nullptr,
        (VoidUnaryOp) trait_cleanup,
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
        &trait_actions,
        nullptr,
        nullptr,
        nullptr
};

bool AddDefaultProperties(Trait *trait, String *name, String *doc, Tuple *mro) {
#define ADD_PROPERTY(key, value) \
    if(!NamespaceNewSymbol(trait->names, key, (ArObject *) value, PropertyInfo(PropertyType::PUBLIC|PropertyType::CONST | PropertyType::STATIC))) \
        return false

    ADD_PROPERTY("__name", name);
    ADD_PROPERTY("__doc", doc);
    ADD_PROPERTY("__mro", mro);

    return true;

#undef ADD_PROPERTY
}

Trait *argon::object::TraitNew(String *name, Namespace *names, Trait **bases, ArSize count) {
    Trait *trait;
    List *bases_list;
    List *lmro;

    Tuple *mro = nullptr;

    if ((trait = ArObjectNew<Trait>(RCType::INLINE, &type_trait_)) != nullptr) {
        if (count > 0) {
            if ((bases_list = BuildBasesList(bases, count)) != nullptr) {
                lmro = ComputeMRO(bases_list);
                Release(bases_list);

                if (lmro == nullptr) {
                    Release(trait);
                    return nullptr;
                }

                if (lmro->len == 0) {
                    Release(lmro);
                    Release(trait);
                    return nullptr;
                }

                mro = TupleNew(lmro);
                Release(lmro);

                if (mro == nullptr) {
                    Release(trait);
                    return nullptr;
                }
            }


        }

        trait->names = IncRef(names);
        trait->mro = IncRef(mro);

        AddDefaultProperties(trait, name, nullptr, mro);
    }

    return trait;
}

// Calculate MRO with C3 linearization
// WARNING: This function uses List object in raw mode!
// NO IncRef or Release will be made during elaboration.
// TODO: move from List to LinkedList or iterator!
/*
 * T1  T2  T3  T4  T5  T6  T7  T8  T9  ...  TN
 * ^  ^                                       ^
 * |  +---------------------------------------+
 * |                   Tail
 * +--Head
 */
List *argon::object::ComputeMRO(List *bases) {
    size_t hlist_idx = 0;
    List *output;

    if ((output = ListNew(bases->len)) == nullptr)
        return nullptr;

    while (hlist_idx < bases->len) {
        // Get head list
        auto head_list = (List *) bases->objects[hlist_idx];

        if (head_list->len > 0) {
            // Get head of head_list
            auto head = head_list->objects[0];

            // Check if head is in the tail of any other lists
            for (size_t i = 0; i < bases->len; i++) {
                if (hlist_idx != i) {
                    auto tail_list = ((List *) bases->objects[i]);

                    for (size_t j = 1; j < tail_list->len; j++) {
                        auto target = (Trait *) tail_list->objects[j];
                        if (head == target)
                            goto next_head;
                    }
                }
            }

            // If the current head is equal to head of another list, remove it!
            for (size_t i = 0; i < bases->len; i++) {
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

    // if len(output) == 0 no good head was found (Ehm, yes this is a user error... warn him!)
    return output;
}

List *argon::object::BuildBasesList(Trait **traits, ArSize count) {
    List *bases;
    List *tmp;

    ArSize cap;

    if ((bases = ListNew(count)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < count; i++) {
        cap = 1;

        if (traits[i]->mro != nullptr)
            cap += traits[i]->mro->len;

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
        if (!ListAppend(tmp, traits[i]))
            goto error;
        // ******************************

        if (traits[i]->mro != nullptr) {
            if (!ListConcat(tmp, traits[i]->mro))
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