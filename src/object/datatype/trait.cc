// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "trait.h"

using namespace argon::object;
using namespace argon::memory;

void trait_cleanup(Trait *self) {
    Release(self->name);
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
        nullptr,
        nullptr,
        nullptr
};

Trait *argon::object::TraitNew(String *name, Namespace *names, List *mro) {
    auto trait = ArObjectNew<Trait>(RCType::INLINE, &type_trait_);

    if (trait != nullptr) {
        IncRef(name);
        trait->name = name;
        IncRef(names);
        trait->names = names;
        IncRef(mro);
        trait->mro = mro;
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

List *argon::object::BuildBasesList(Trait **traits, size_t count) {
    List *bases;

    if ((bases = ListNew(count)) == nullptr)
        return nullptr;

    /*
     * MRO list should contain the trait itself as the first element,
     * this would cause a circular reference!
     * To avoid this, the trait itself is excluded from the MRO list.
     *
     * To perform the calculation, however, it must be included!
     * Therefore it is added during the generation of the list of base traits.
     */
    for (size_t i = 0; i < count; i++) {
        if (traits[i]->mro == nullptr) {
            List *tmp = ListNew(1);

            if (tmp == nullptr) {
                Release(bases);
                return nullptr;
            }

            if (!ListAppend(tmp, traits[i])) {
                Release(tmp);
                Release(bases);
                return nullptr;
            }

            if (!ListAppend(bases, tmp)) {
                Release(tmp);
                Release(bases);
                return nullptr;
            }

            Release(tmp);
        } else {
            List *tmp = ListNew(traits[i]->mro->len + 1);

            if (tmp == nullptr) {
                Release(bases);
                return nullptr;
            }

            if(!ListAppend(tmp, traits[i])){
                Release(tmp);
                Release(bases);
                return nullptr;
            }

            if(!ListConcat(tmp, traits[i]->mro)){
                Release(tmp);
                Release(bases);
                return nullptr;
            }

            if (!ListAppend(bases, tmp)) {
                Release(tmp);
                Release(bases);
                return nullptr;
            }

            Release(tmp);
        }
    }

    return bases;
}