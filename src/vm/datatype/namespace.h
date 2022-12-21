// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_NAMESPACE_H_
#define ARGON_VM_DATATYPE_NAMESPACE_H_

#include <vm/sync/rsm.h>

#include <util/macros.h>

#include "arobject.h"
#include "hashmap.h"

namespace argon::vm::datatype {
    enum class AttributeFlag {
        // Behaviour
        CONST = 1,
        WEAK = 1 << 1,

        // Visibility
        PUBLIC = 1 << 2
    };
}

ENUMBITMASK_ENABLE(argon::vm::datatype::AttributeFlag);

namespace argon::vm::datatype {
    struct AttributeProperty {
        AttributeFlag flags;

        [[nodiscard]] bool IsConstant() const {
            return ENUMBITMASK_ISTRUE(this->flags, AttributeFlag::CONST);
        };

        [[nodiscard]] bool IsPublic() const {
            return ENUMBITMASK_ISTRUE(this->flags, AttributeFlag::PUBLIC);
        }

        [[nodiscard]] bool IsWeak() const {
            return ENUMBITMASK_ISTRUE(this->flags, AttributeFlag::WEAK);
        }
    };

    struct PropertyStore {
        RefStore value;

        AttributeProperty properties;
    };

    using NSEntry = HEntry<ArObject, PropertyStore>;

    struct Namespace {
        AROBJ_HEAD;

        sync::RecursiveSharedMutex rwlock;

        HashMap<ArObject, PropertyStore> ns;
    };
    extern const TypeInfo *type_namespace_;

    ArObject *NamespaceLookup(Namespace *ns, ArObject *key, AttributeProperty *out_aprop);

    /**
     * @brief Checks if the namespace contains the key and returns its AttributeProperty.
     *
     * @param ns Pointer to namespace.
     * @param key Pointer to the key.
     * @param out_aprop Pointer to an AttributeProperty writable object.
     * @return True if key exists, false otherwise.
     */
    bool NamespaceContains(Namespace *ns, ArObject *key, AttributeProperty *out_aprop);

    /**
     * @brief Add new symbol to namespace.
     *
     * @param ns Pointer to namespace.
     * @param key Pointer to the key.
     * @param value Pointer to the value to be added.
     * @param aa AttributeFlag
     * @return True on success, false otherwise.
     */
    bool NamespaceNewSymbol(Namespace *ns, ArObject *key, ArObject *value, AttributeFlag aa);

    /**
     * @brief Add new symbol to namespace.
     *
     * @param ns Pointer to namespace.
     * @param key Pointer to the C-string that represent the key.
     * @param value Pointer to the value to be added.
     * @param aa AttributeFlag
     * @return True on success, false otherwise.
     */
    bool NamespaceNewSymbol(Namespace *ns, const char *key, ArObject *value, AttributeFlag aa);

    /**
     * @brief Replaces the value associated with a key.
     *
     * @param ns Pointer to namespace.
     * @param key Pointer to the key.
     * @param value Pointer to the value to be added.
     * @return True if the value has been replaced, false otherwise (The key does not exist).
     */
    bool NamespaceSet(Namespace *ns, ArObject *key, ArObject *value);

    /**
     * @brief Create new namespace.
     *
     * @return A pointer to new namespace, otherwise nullptr.
     */
    Namespace *NamespaceNew();

    /**
     * @brief Cleans the namespace.
     *
     * @param ns Pointer to namespace.
     */
    void NamespaceClear(Namespace *ns);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_NAMESPACE_H_
