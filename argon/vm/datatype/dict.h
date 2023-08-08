// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_DICT_H_
#define ARGON_VM_DATATYPE_DICT_H_

#include <cstring>

#include <argon/vm/sync/rsm.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/iterator.h>
#include <argon/vm/datatype/hashmap.h>

namespace argon::vm::datatype {
    struct Dict {
        AROBJ_HEAD;

        sync::RecursiveSharedMutex rwlock;

        HashMap<ArObject, ArObject *> hmap;
    };
    extern const TypeInfo *type_dict_;

    using DictIterator = CursorIterator<Dict, HEntry<ArObject, ArObject *>>;
    extern const TypeInfo *type_dict_iterator_;

    /**
     * @brief Look for the element \p key.
     *
     * @param dict Pointer to an instance of dict.
     * @param key Pointer to an object to use as a key.
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    ArObject *DictLookup(Dict *dict, ArObject *key);

    /**
     * @brief Look for the element \p key.
     *
     * @param dict Pointer to an instance of dict.
     * @param key Pointer to C-string to use as a key.
     * @param length The length of the C-string.
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    ArObject *DictLookup(Dict *dict, const char *key, ArSize length);

    /**
     * @brief Look for the element \p key.
     *
     * @param dict Pointer to an instance of dict.
     * @param key Pointer to C-string to use as a key.
     * @return A pointer to the object on success,
     * in case of error nullptr will be returned and the panic state will be set.
     */
    inline ArObject *DictLookup(Dict *dict, const char *key) {
        return DictLookup(dict, key, strlen(key));
    }

    /**
     * @brief Insert an element into the dict.
     *
     * @param dict Pointer to an instance of dict.
     * @param key Pointer to an object to use as a key.
     * @param value Value to insert.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool DictInsert(Dict *dict, ArObject *key, ArObject *value);

    /**
     * @brief Insert an element into the dict.
     *
     * @param dict Pointer to an instance of dict.
     * @param key Pointer to C-string to use as a key.
     * @param value Value to insert.
     * @return True on success, in case of error false will be returned and the panic state will be set.
     */
    bool DictInsert(Dict *dict, const char *key, ArObject *value);

    /**
     * @brief Convenience function to look up an Bool type (useful when used with the kwargs function parameter).
     *
     * @param dict Pointer to an instance of dict.
     * @param key Pointer to C-string to use as a key.
     * @param _default Default value to return if the lookup fails or if the object is not of the expected type.
     * @return The value obtained by searching for key, otherwise the default value.
     */
    bool DictLookupIsTrue(Dict *dict, const char *key, bool _default);

    /**
     * @brief Remove an element from the dict.
     *
     * @param dict Pointer to an instance of dict.
     * @param key Pointer to an object to use as a key.
     * @return True if the object was found and deleted, in the event of an error or object not found,
     * false will be returned and a panic state will be set (only in case of problems with key hashing).
     */
    bool DictRemove(Dict *dict, ArObject *key);

    /**
     * @brief Remove an element from the dict.
     *
     * @param dict Pointer to an instance of dict.
     * @param key Pointer to C-string to use as a key.
     * @return True if the object was found and deleted, in the event of an error or object not found,
     * false will be returned and a panic state will be set (only in case of problems with key hashing).
     */
    bool DictRemove(Dict *dict, const char *key);

    /***
     * @brief Create a dictionary by merging two dictionaries together.
     *
     * @warning If a key collision occurs during dictionary merge,
     * the operation will be aborted and panic will be set.
     *
     * @param dict1 Pointer to first dict.
     * @param dict2 Pointer to second dict.
     * @param clone Indicates, in case one of the two dicts is nil/nullptr,
     * whether to return a reference to the other dictionary or to clone it.
     *
     * @return A pointer to new merged dict, otherwise nullptr will be returned and a panic state will be set.
     */
    Dict *DictMerge(Dict *dict1, Dict *dict2, bool clone);

    /**
     * @brief Create a new dict.
     *
     * @return A pointer to a dict object, otherwise nullptr.
     */
    Dict *DictNew();

    /**
     * @brief Create a new dict from an iterable object.
     *
     * @param object Pointer to an iterable object.
     * @return A pointer to a dict object, otherwise nullptr.
     */
    Dict *DictNew(ArObject *object);

    /**
     * @brief Create a new dictionary of the desired initial size.
     *
     * @return A pointer to a dict object, otherwise nullptr.
     */
    Dict *DictNew(unsigned int size);

    /**
     * @brief Convenience function to look up an Int type (useful when used with the kwargs function parameter).
     *
     * @param dict Pointer to an instance of dict.
     * @param key Pointer to C-string to use as a key.
     * @param _default Default value to return if the lookup fails or if the object is not of the expected type.
     * @return The value obtained by searching for key, otherwise the default value.
     */
    IntegerUnderlying DictLookupInt(Dict *dict, const char *key, IntegerUnderlying _default);

    /**
     * @brief Delete the contents of the entire dict.
     *
     * @param dict Pointer to an instance of dict.
     */
    void DictClear(Dict *dict);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_DICT_H_
