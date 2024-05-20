// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_NAMESPACE_H_
#define ARGON_VM_DATATYPE_NAMESPACE_H_

#include <argon/vm/sync/rsm.h>

#include <argon/util/macros.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/hashmap.h>
#include <argon/vm/datatype/list.h>
#include <argon/vm/datatype/set.h>

#undef CONST
#undef PUBLIC
#undef WEAK

namespace argon::vm::datatype {
    enum class AttributeFlag {
        // Behaviour
        CONST = 1,
        WEAK = 1 << 1,

        // Visibility
        PUBLIC = 1 << 2,

        // Misc
        NON_COPYABLE = 1 << 3
    };
}

ENUMBITMASK_ENABLE(argon::vm::datatype::AttributeFlag);

namespace argon::vm::datatype {
    struct AttributeProperty {
        AttributeFlag flags;

        [[nodiscard]] bool IsConstant() const {
            return ENUMBITMASK_ISTRUE(this->flags, AttributeFlag::CONST);
        };

        [[nodiscard]] bool IsNonCopyable() const {
            return ENUMBITMASK_ISTRUE(this->flags, AttributeFlag::NON_COPYABLE);
        }

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
    _ARGONAPI extern const TypeInfo *type_namespace_;

    /**
     * @brief @brief Look for the element \p key.
     *
     * @param ns Pointer to an instance of Namespace.
     * @param key Pointer to an object to use as a key.
     * @param out_aprop Pointer to AttributeProperty that can receive object properties.
     * @return A pointer to the object on success, in case of error nullptr will be returned.
     */
    ArObject *NamespaceLookup(Namespace *ns, ArObject *key, AttributeProperty *out_aprop);

    /**
     * @brief @brief Look for the element \p key.
     *
     * @param ns Pointer to an instance of Namespace.
     * @param key Pointer to C-string to use as a key.
     * @param out_aprop Pointer to AttributeProperty that can receive object properties.
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    ArObject *NamespaceLookup(Namespace *ns, const char *key, AttributeProperty *out_aprop);

    /**
     * @brief Checks if the namespace contains the key and returns its AttributeProperty.
     *
     * @param ns Pointer to namespace.
     * @param key Pointer to the key.
     * @param out_aprop Pointer to AttributeProperty that can receive object properties.
     * @return True if key exists, false otherwise.
     */
    bool NamespaceContains(Namespace *ns, ArObject *key, AttributeProperty *out_aprop);

    /**
     * @brief Checks if the namespace contains the key and returns its AttributeProperty.
     *
     * @param ns Pointer to namespace.
     * @param key Pointer to the key.
     * @param out_aprop Pointer to AttributeProperty that can receive object properties.
     * @param out_exists Pointer to a bool variable that indicates whether the searched object exists or not.
     * @return True on success, false otherwise (panic state will be set).
     */
    bool NamespaceContains(Namespace *ns, const char *key, AttributeProperty *out_aprop, bool *out_exists);

    /**
     * @brief Merges the public contents of two namespaces.
     *
     * @param dest Destination namespace.
     * @param src Source namespace.
     * @return True if successful, otherwise returns false.
     */
    bool NamespaceMergePublic(Namespace *dest, Namespace *src);

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
     * @brief Add new string to namespace.
     *
     * @param ns Pointer to namespace.
     * @param key Pointer to the C-string that represent the key.
     * @param value Pointer to the C-string that represent the string value.
     * @param aa AttributeFlag
     * @return True on success, false otherwise.
     */
    bool NamespaceNewSymbol(Namespace *ns, const char *key, const char *value, AttributeFlag aa);

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
     * @brief Replaces the value at a given position.
     *
     * @param ns Pointer to namespace.
     * @param values Array containing the values to replace.
     * @param count Number of elements in the values array.
     * @return True if the value has been replaced, false otherwise (The key does not exist).
     */
    bool NamespaceSetPositional(Namespace *ns, ArObject **values, ArSize count);

    /**
     * @brief Create a list of namespace keys.
     *
     * @param ns Pointer to namespace.
     * @param match Filters the keys by specifying which attributes they must have in order to be exported.
     * @return List of namespace keys.
     */
    List *NamespaceKeysToList(Namespace *ns, AttributeFlag match);

    /**
     * @brief Create new namespace.
     *
     * @return A pointer to new namespace, otherwise nullptr.
     */
    Namespace *NamespaceNew();

    /**
     * @brief Clone an existing namespace.
     *
     * @param ns Pointer to namespace.
     * @param ignore Filter that specifies which items to ignore when copying.
     * @return A pointer to new namespace, otherwise nullptr.
     */
    Namespace *NamespaceNew(Namespace *ns, AttributeFlag ignore);

    /**
     * @brief Create a Set of namespace keys.
     *
     * @param ns Pointer to namespace.
     * @param match Filters the keys by specifying which attributes they must have in order to be exported.
     * @return Set of namespace keys.
     */
    Set *NamespaceKeysToSet(Namespace *ns, AttributeFlag match);

    /**
     * @brief Cleans the namespace.
     *
     * @param ns Pointer to namespace.
     */
    [[maybe_unused]] void NamespaceClear(Namespace *ns);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_NAMESPACE_H_
